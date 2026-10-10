#include "app/services/net/NetService.h"
#include "app/services/api/ApiCatalog.h"
#include "app/services/net/NetAccountStore.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QTimer>

#include <memory>

namespace miacode {

NetService::NetService(JobRegistry& jobs, net::NetEnginePort& engine, QObject* parent)
    : QObject(parent), jobs_(jobs), engine_(engine) {}

NetService::~NetService()
{
    // Detach callbacks before cancellation can synchronously finish a reply.
    disconnect(&jobs_, nullptr, this, nullptr);
    const auto cancellations = cancellations_;
    cancellations_.clear();
    for (const auto& cancel : cancellations) if (cancel) cancel();
}

QString NetService::kindForOperation(const QString& operation)
{
    return operation.startsWith(QStringLiteral("net."))
        ? api::ApiCatalog::operation(operation).value(QStringLiteral("jobKind")).toString() : QString{};
}

net::NetCallResult NetService::start(const QString& principal, const QString& operation,
                                    const QJsonObject& input, const QString& idempotencyKey)
{
    const QString kind = kindForOperation(operation);
    if (!idempotencyKey.isEmpty() && !api::ApiCatalog::validRequestKey(idempotencyKey))
        return {{}, jobError(QStringLiteral("request.invalid"))};
    if (principal.isEmpty() || kind.isEmpty() || !supports(operation)) {
        return {{}, jobError(QStringLiteral("capability.unavailable"))};
    }
    const auto validation = api::ApiCatalog::validate(operation, input);
    if (!validation.ok()) return {{}, validation.error};
    const auto parameters = validation.parameters;
    const auto policy = api::ApiCatalog::operation(operation).value(QStringLiteral("idempotency")).toString();
    const bool idempotent = policy == QLatin1String("required");
    if (idempotent && idempotencyKey.trimmed().isEmpty()) return {{}, jobError(QStringLiteral("request.invalid"))};
    const QString key = principal + QChar(0) + operation + QChar(0) + idempotencyKey;
    // Secret-bearing login requests never enter the idempotency ledger.
    const bool recordKey = !idempotencyKey.isEmpty() && policy != QLatin1String("none");
    const auto fingerprint = recordKey ? QCryptographicHash::hash(
        QJsonDocument(parameters).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256) : QByteArray{};
    if (recordKey && idempotency_.contains(key)) {
        const auto previous = idempotency_.value(key);
        if (previous.fingerprint != fingerprint) return {{}, jobError(QStringLiteral("idempotency.conflict"))};
        const auto snapshot = jobs_.snapshot(previous.jobId, principal);
        return {QJsonObject{{QStringLiteral("jobId"), previous.jobId}, {QStringLiteral("kind"), kind},
            {QStringLiteral("state"), snapshot.value(QStringLiteral("state"))}}, {}};
    }
    QJsonArray inputs;
    if (operation == QLatin1String("net.downloads.create")) {
        for (const auto& value : parameters.value(QStringLiteral("chartIds")).toArray()) {
            inputs.append(QJsonObject{{QStringLiteral("chartRef"), value}});
        }
    } else if (operation == QLatin1String("net.uploads.create")) {
        const auto parentJobId = parameters.value("parentJobId").toString();
        const auto parent = jobs_.snapshot(parentJobId, principal);
        if (!parentJobId.isEmpty() && (parent.value("kind") != QLatin1String("net.upload")
            || !JobRegistry::isTerminal(parent.value("state").toString()))) return {{}, jobError("job.invalid_state")};
        for (const auto& value : parameters.value(QStringLiteral("orderedItemIds")).toArray()) {
            QJsonObject input{{QStringLiteral("materialRef"), value}};
            if (!parentJobId.isEmpty()) {
                bool allowed = false;
                for (const auto& previous : jobs_.items(parentJobId, principal)) {
                    const auto item = previous.toObject();
                    if (item.value("materialRef") != value || item.value("state") == QLatin1String("succeeded")
                        || item.value("state") == QLatin1String("outcome_unknown")) continue;
                    allowed = true;
                    input.insert("parentItemId", item.value("itemId"));
                    break;
                }
                if (!allowed) return {{}, jobError("request.invalid")};
            }
            inputs.append(input);
        }
    }
    if ((operation == QLatin1String("net.downloads.create") || operation == QLatin1String("net.uploads.create"))
        && inputs.isEmpty()) return {{}, jobError(QStringLiteral("request.invalid"))};
    const auto token = jobs_.create(principal, kind, inputs, parameters.value("parentJobId").toString());
    if (!token.isValid()) return {{}, jobError(QStringLiteral("request.invalid"))};
    if (recordKey) idempotency_.insert(key, {fingerprint, token.jobId});
    net::NetTaskRequest request{principal, operation, parameters, jobs_.itemIds(token)};
    QPointer<NetService> self(this);
    // The request may contain a transient password, held only until the engine accepts it.
    QTimer::singleShot(0, this, [self, token, request = std::move(request)]() mutable {
        if (self) self->launch(token, std::move(request));
    });
    return {QJsonObject{{QStringLiteral("jobId"), token.jobId}, {QStringLiteral("kind"), kind},
        {QStringLiteral("state"), QStringLiteral("queued")}}, {}};
}

void NetService::launch(const JobToken& token, net::NetTaskRequest request)
{
    if (!jobs_.setState(token, QStringLiteral("running"), QStringLiteral("starting"))) return;
    QPointer<NetService> self(this);
    const auto savedRequest = request.operation == QLatin1String("net.accounts.login")
        ? net::NetTaskRequest{} : request;
    const auto resultSchema = api::ApiCatalog::operation(request.operation).value(QStringLiteral("jobResultSchema")).toString();
    const auto principal = request.principal;
    const bool login = request.operation == QLatin1String("net.accounts.login");
    const bool remember = login && request.parameters.value("remember").toBool();
    const auto credential = remember ? QPair<QString, QString>{request.parameters.value("username").toString(), request.parameters.value("password").toString()}
        : QPair<QString, QString>{};
    const auto completed = std::make_shared<bool>(false);
    const auto execute = [this](net::NetTaskRequest request, net::NetEnginePort::Event event, net::NetEnginePort::Done done) {
        if (request.operation == QLatin1String("net.previews.open")) return previewOpen_(std::move(request), std::move(done));
        return engine_.execute(std::move(request), std::move(event), std::move(done));
    };
    auto cancel = execute(std::move(request),
        [self, token](net::NetTaskEvent event) {
            if (!self || !self->jobs_.accepts(token)) return;
            if (event.itemIndex < 0) {
                self->jobs_.setState(token, event.state, event.phase, event.error);
            } else {
                const auto ids = self->jobs_.itemIds(token);
                if (event.itemIndex >= ids.size()) return;
                self->jobs_.updateItem(token, ids[event.itemIndex], event.state, event.phase,
                    event.error, event.progress, event.artifacts);
            }
        },
        [self, token, savedRequest, completed, resultSchema, principal, login, remember, credential](net::NetTaskResult outcome) {
            *completed = true;
            if (!self || !self->jobs_.accepts(token)) return;
            self->cancellations_.remove(token.jobId);
            const bool cancelling = self->jobs_.snapshot(token.jobId, principal).value("state") == QLatin1String("cancelling");
            if (login && outcome.error.isEmpty() && !cancelling) {
                auto account = outcome.result.toObject();
                const bool stored = remember && NetAccountStore::save(principal, credential.first, credential.second);
                if (!remember) NetAccountStore::erase(principal);
                account.insert("remembered", stored);
                self->engine_.setAccountRemembered(principal, account.value("accountRef").toString(), stored);
                if (remember && !stored) account.insert("storageError", "credential_store.unavailable");
                outcome.result = account;
            }
            if (!outcome.blocked && outcome.error.isEmpty()
                && self->jobs_.snapshot(token.jobId, principal).value(QStringLiteral("state")) != QLatin1String("cancelling")) {
                if (!api::ApiCatalog::validateResult(resultSchema, outcome.result)) {
                    outcome.result = QJsonValue::Null;
                    outcome.error = jobError(QStringLiteral("internal.contract_violation"));
                }
            }
            if (outcome.blocked && !savedRequest.operation.isEmpty()) {
                auto resumeRequest = savedRequest;
                resumeRequest.parameters.insert("previousResult", outcome.result);
                self->resumable_.insert(token.jobId, resumeRequest);
                self->jobs_.setState(token, QStringLiteral("blocked"), QStringLiteral("blocked"), outcome.error);
                self->jobs_.setCancelHandler(token, [self, token] { if (self) self->jobs_.finish(token); });
            } else {
                self->resumable_.remove(token.jobId);
                self->jobs_.finish(token, outcome.result, outcome.error);
            }
        });
    if (*completed || !jobs_.accepts(token)) return;
    cancellations_.insert(token.jobId, cancel);
    jobs_.setCancelHandler(token, std::move(cancel));
}

net::NetCallResult NetService::call(const QString& principal, const QString& operation,
                                   const QJsonObject& input)
{
    const auto validation = api::ApiCatalog::validate(operation, input);
    if (!validation.ok()) return {{}, validation.error};
    const auto parameters = validation.parameters;
    if (operation == QLatin1String("document.snapshot") && documentSnapshot_) return {documentSnapshot_(), {}};
    if (operation == QLatin1String("jobs.resume")) {
        const QString jobId = parameters.value(QStringLiteral("jobId")).toString();
        if (!resumable_.contains(jobId)) return {{}, jobError(QStringLiteral("job.invalid_state"))};
        if (resumable_.value(jobId).operation != QLatin1String("net.downloads.create"))
            return {{}, jobError(QStringLiteral("job.invalid_state"))};
        const auto token = jobs_.resume(jobId, principal,
            parameters.value(QStringLiteral("expectedJobVersion")).toString().toULongLong());
        if (!token.isValid()) return {{}, jobError(QStringLiteral("job.invalid_state"))};
        auto request = resumable_.take(jobId);
        const auto items = jobs_.items(jobId, principal);
        QJsonArray pending;
        for (const auto& item : items) {
            const auto value = item.toObject();
            const auto state = value.value(QStringLiteral("state")).toString();
            if (state == QLatin1String("pending") || state == QLatin1String("blocked") || state == QLatin1String("running") || state == QLatin1String("retry_wait")) {
                pending.append(value.value(QStringLiteral("inputOrder")));
            }
        }
        request.parameters.insert(QStringLiteral("pendingIndices"), pending);
        launch(token, std::move(request));
        return {jobs_.snapshot(jobId, principal), {}};
    }
    return engine_.call(principal, operation, parameters);
}

} // namespace miacode
