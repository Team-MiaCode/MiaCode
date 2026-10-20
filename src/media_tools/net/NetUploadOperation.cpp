#include "media_tools/net/NetProvider.h"
#include "media_tools/net/NetUploadDiagnostics.h"
#include "common/DebugLog.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QPointer>
#include <QSaveFile>
#include <QTimer>
#include <QUuid>
#include <QtConcurrentRun>
#include <atomic>
#include <limits>

namespace miacode::net {
namespace {
QString reference(const char* prefix) { return QString::fromLatin1(prefix) + QUuid::createUuid().toString(QUuid::Id128); }
QJsonObject failure(const QString& code) { return {{"code", code}, {"message", code}, {"retryable", false}}; }
QList<QPair<QString, QString>> files(const NetUploadJob& job) {
    QList<QPair<QString, QString>> result{{"chart", job.chartPath}, {"image", job.backgroundPath}, {"track", job.trackPath}};
    if (!job.videoPath.isEmpty()) result.append({"video", job.videoPath});
    return result;
}
QString digest(const QString& path, const std::shared_ptr<std::atomic_bool>& cancelled) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        if (cancelled->load()) return {};
        const auto data = file.read(64 * 1024);
        if (data.isEmpty() && file.error() != QFileDevice::NoError) return {};
        hash.addData(data);
    }
    return QString::fromLatin1(hash.result().toHex());
}
NetUploadResponseAssessment assessment(const NetHttpResponse& response) {
    NetUploadResponseInfo info;
    info.statusCode = response.status;
    info.payload = response.payload;
    info.contentType = QString::fromLatin1(response.contentType);
    info.retryAfter = QString::fromLatin1(response.retryAfter);
    return assessNetUploadResponse(info);
}
qint64 retryDelay(const NetHttpResponse& response) {
    const auto header = QString::fromLatin1(response.retryAfter).trimmed();
    bool numeric = false;
    qint64 seconds = header.toLongLong(&numeric);
    if (!numeric) {
        const auto date = QDateTime::fromString(header, Qt::RFC2822Date);
        seconds = date.isValid() ? QDateTime::currentDateTimeUtc().secsTo(date.toUTC()) : 60;
    }
    if (seconds <= 0) seconds = 60;
    return seconds > 3600 ? std::numeric_limits<qint64>::max() / 2 : seconds * 1000;
}
struct FrozenUpload {
    std::shared_ptr<QTemporaryDir> directory;
    NetUploadJob files;
    bool valid = false;
};
}

class NetAccountOperation final : public QObject {
public:
    NetAccountOperation(NetProvider& provider, NetTaskRequest request, NetEnginePort::Event event, NetEnginePort::Done done)
        : QObject(&provider), provider_(provider), request_(std::move(request)), event_(std::move(event)), done_(std::move(done)), accountRef_(reference("account_")) {}
    ~NetAccountOperation() override { done_ = {}; if (cancelHttp_) cancelHttp_(); }
    void start() {
        if (!done_) return;
        if (request_.parameters.value("providerId") != QLatin1String("majdata")) { finish({{}, failure("capability.unavailable")}); return; }
        http_.url = QUrl(provider_.baseUrl_.toString() + "/account/Login");
        http_.method = "POST";
        http_.accountRef = accountRef_;
        http_.timeoutMs = 90000;
        http_.requireNonEmpty = false;
        http_.headers = {{"Referer", "https://majdata.net/login"}};
        http_.parts = {{"username", request_.parameters.value("username").toString().toUtf8()},
            {"password", QCryptographicHash::hash(request_.parameters.value("password").toString().toUtf8(), QCryptographicHash::Md5).toHex()},
            {"rememberMe", "false"}};
        request_.parameters.remove("password");
        provider_.accountTransfers_.insert(accountRef_, [weak = QPointer<NetAccountOperation>(this)] { if (weak) weak->cancel(); });
        send();
    }
    void cancel() { if (!done_) return; if (cancelHttp_) cancelHttp_(); if (done_) finish({{}, failure("job.cancelled")}); }
private:
    void finish(NetTaskResult result) {
        if (!done_) return;
        provider_.accountTransfers_.remove(accountRef_);
        if (!result.error.isEmpty()) provider_.transport_.releaseAccount(accountRef_);
        auto done = std::move(done_);
        http_.parts.clear();
        cancelHttp_ = {};
        done(std::move(result));
        deleteLater();
    }
    void send() {
        if (!done_) return;
        const auto wait = provider_.accountRetryAt_ - provider_.clock_.elapsed();
        if (wait > 3600000) { finish({{}, failure("upstream.rate_limited")}); return; }
        if (wait > 0) { QTimer::singleShot(int(wait), this, [this] { send(); }); return; }
        if (event_) event_({-1, "running", "login", {}, {}, {}});
        QPointer<NetAccountOperation> self(this);
        cancelHttp_ = provider_.transport_.request(http_, [self](NetHttpResponse response) {
            if (!self || !self->done_) return;
            self->cancelHttp_ = {};
            const auto code = response.error.value("code").toString();
            if (code == QLatin1String("upstream.rate_limited") && self->attempt_++ == 0) {
                const auto delay = retryDelay(response);
                self->provider_.accountRetryAt_ = self->provider_.clock_.elapsed() + delay;
                if (delay > 3600000) { self->finish({{}, response.error}); return; }
                if (self->event_) self->event_({-1, "retry_wait", "login_retry", response.error, {}, {}});
                QTimer::singleShot(int(delay), self, [self] { if (self && self->done_) self->send(); });
                return;
            }
            const auto parsed = assessment(response);
            if (!response.error.isEmpty() || parsed.isApplicationError || parsed.isHtml || response.status != 200) {
                self->finish({{}, response.error.isEmpty() ? failure("upstream.auth_required") : response.error}); return;
            }
            QJsonObject account{{"accountRef", self->accountRef_}, {"providerId", "majdata"},
                {"displayName", self->request_.parameters.value("username")}, {"authenticated", true},
                {"expiresAt", QJsonValue::Null}, {"remembered", false}};
            self->provider_.accounts_.insert(self->accountRef_, {self->request_.principal, account});
            self->finish({account, {}});
        });
    }
    NetProvider& provider_;
    NetTaskRequest request_;
    NetEnginePort::Event event_;
    NetEnginePort::Done done_;
    QString accountRef_;
    NetHttpRequest http_;
    NetTransportPort::Cancel cancelHttp_;
    int attempt_ = 0;
};

class NetScanOperation final : public QObject {
public:
    struct ScanResult { QList<NetProvider::UploadMaterial> materials; QJsonArray rejectedEntries; bool tooMany = false; };
    NetScanOperation(NetProvider& provider, NetTaskRequest request, NetEnginePort::Done done)
        : QObject(&provider), provider_(provider), request_(std::move(request)), done_(std::move(done)), cancelled_(std::make_shared<std::atomic_bool>(false)) {}
    ~NetScanOperation() override { done_ = {}; cancelled_->store(true); if (worker_.isRunning()) worker_.waitForFinished(); }
    void cancel() { cancelled_->store(true); }
    void start() {
        const auto grant = provider_.directoryGrants_.value(request_.parameters.value("rootGrantRef").toString());
        if (grant.principal != request_.principal) { finish({{}, failure("permission.denied")}); return; }
        const auto previous = request_.parameters.value("uploadPlanRef").toString();
        if (!previous.isEmpty()) {
            const auto it = provider_.uploadPlans_.constFind(previous);
            if (it == provider_.uploadPlans_.cend() || it->principal != request_.principal || it->expiresAt <= provider_.clock_.elapsed()) {
                finish({{}, failure("query.expired")}); return;
            }
            const auto retained = request_.parameters.value("retainedItemIds");
            for (const auto& material : it->materials) {
                if (retained.isUndefined() || retained.toArray().contains(material.descriptor.value("itemId"))) materials_.append(material);
            }
        }
        const auto cancelled = cancelled_;
        worker_.setFuture(QtConcurrent::run([grant, cancelled] {
            ScanResult result;
            QList<NetUploadScanRejection> rejected;
            const auto candidates = scanNetUploadFolders(grant.path, &rejected);
            if (candidates.size() + rejected.size() > 500) { result.tooMany = true; return result; }
            for (const auto& entry : rejected) result.rejectedEntries.append(QJsonObject{{"displayName", entry.displayName}, {"reason", entry.reason}});
            for (const auto& job : candidates) {
                if (cancelled->load()) break;
                QJsonArray metadata;
                QString reason;
                for (const auto& [kind, path] : files(job)) {
                    const QFileInfo before(path);
                    const auto relative = QDir(grant.path).relativeFilePath(before.canonicalFilePath());
                    if (relative.startsWith("../") || QDir::isAbsolutePath(relative)) { reason = "permission.denied"; break; }
                    const auto hash = digest(path, cancelled);
                    const QFileInfo after(path);
                    if (hash.isEmpty()) { reason = "file.unreadable"; break; }
                    if (before.size() != after.size() || before.lastModified() != after.lastModified()) { reason = "file.changed"; break; }
                    metadata.append(QJsonObject{{"kind", kind}, {"displayName", before.fileName()}, {"bytes", QString::number(before.size())},
                        {"modifiedAt", before.lastModified().toUTC().toString(Qt::ISODateWithMs)}, {"sha256", hash}});
                }
                if (reason.isEmpty()) result.materials.append({job, QJsonObject{{"itemId", reference("material_")}, {"materialRef", reference("material_")},
                    {"displayName", job.displayName}, {"files", metadata}}});
                else result.rejectedEntries.append(QJsonObject{{"displayName", job.displayName}, {"reason", reason}});
            }
            return result;
        }));
        connect(&worker_, &QFutureWatcher<ScanResult>::finished, this, [this] {
            if (cancelled_->load()) { finish({{}, failure("job.cancelled")}); return; }
            const auto scan = worker_.result();
            if (scan.tooMany) { finish({{}, failure("request.too_large")}); return; }
            for (const auto& material : scan.materials) {
                bool duplicate = false;
                for (const auto& existing : materials_) if (QFileInfo(existing.files.directoryPath).canonicalFilePath() == QFileInfo(material.files.directoryPath).canonicalFilePath()) duplicate = true;
                if (!duplicate) materials_.append(material);
            }
            if (materials_.size() > 500) { finish({{}, failure("request.too_large")}); return; }
            const auto planRef = reference("plan_");
            provider_.uploadPlans_.insert(planRef, {request_.principal, materials_, provider_.clock_.elapsed() + 3600000});
            QJsonArray items;
            for (const auto& material : materials_) items.append(material.descriptor);
            finish({QJsonObject{{"uploadPlanRef", planRef}, {"items", items}, {"rejectedEntries", scan.rejectedEntries},
                {"expiresAt", QDateTime::currentDateTimeUtc().addSecs(3600).toString(Qt::ISODateWithMs)}}, {}});
        });
    }
private:
    void finish(NetTaskResult result) { if (!done_) return; auto done = std::move(done_); done(std::move(result)); deleteLater(); }
    NetProvider& provider_;
    NetTaskRequest request_;
    NetEnginePort::Done done_;
    std::shared_ptr<std::atomic_bool> cancelled_;
    QFutureWatcher<ScanResult> worker_;
    QList<NetProvider::UploadMaterial> materials_;
};

class NetUploadOperation final : public QObject {
public:
    NetUploadOperation(NetProvider& provider, NetTaskRequest request, NetEnginePort::Event event, NetEnginePort::Done done)
        : QObject(&provider), provider_(provider), request_(std::move(request)), event_(std::move(event)), done_(std::move(done)),
          cancelled_(std::make_shared<std::atomic_bool>(false)), accountRef_(request_.parameters.value("accountRef").toString()) {}
    ~NetUploadOperation() override { done_ = {}; cancelled_->store(true); if (cancelHttp_) cancelHttp_(); if (validation_.isRunning()) validation_.waitForFinished(); }
    void start() {
        if (!done_) return;
        const auto account = provider_.accounts_.value(accountRef_);
        const auto plan = provider_.uploadPlans_.value(request_.parameters.value("uploadPlanRef").toString());
        if (account.principal != request_.principal || plan.principal != request_.principal) { finish(failure("permission.denied")); return; }
        if (plan.expiresAt <= provider_.clock_.elapsed()) { finish(failure("query.expired")); return; }
        if (provider_.uploadActive_ || provider_.accountTransfers_.contains(accountRef_)) { finish(failure("job.invalid_state")); return; }
        for (const auto& id : request_.parameters.value("orderedItemIds").toArray()) {
            bool found = false;
            for (const auto& material : plan.materials) if (material.descriptor.value("itemId") == id) { materials_.append(material); found = true; break; }
            if (!found) { finish(failure("request.invalid")); return; }
        }
        if (!log("queue_start")) { finish(failure("diagnostic.unavailable")); return; }
        provider_.accountTransfers_.insert(accountRef_, [weak = QPointer<NetUploadOperation>(this)] { if (weak) weak->cancel(); });
        ownsTransfer_ = true;
        provider_.uploadActive_ = true;
        next();
    }
    void cancel() {
        cancelled_->store(true);
        if (cancelHttp_) cancelHttp_();
        if (done_ && !validation_.isRunning()) finish(failure("job.cancelled"));
    }
private:
    bool log(const QString& phase, const NetHttpResponse& response = {}) const {
        return debug_log::appendNetUploadEvent(QStringLiteral("phase=%1 item=%2 status=%3 code=%4")
            .arg(phase, index_ >= 0 && index_ < request_.itemIds.size() ? request_.itemIds[index_] : QString{})
            .arg(response.status).arg(response.error.value("code").toString()));
    }
    void emitItem(const QString& state, const QString& phase, const QJsonObject& error = {}, const QJsonObject& progress = {}) {
        if (event_) event_({index_, state, phase, error, progress, {}});
    }
    void finish(QJsonObject error = {}, bool blocked = false) {
        if (!done_) return;
        if (ownsTransfer_) { provider_.accountTransfers_.remove(accountRef_); provider_.uploadActive_ = false; }
        auto done = std::move(done_);
        cancelHttp_ = {};
        QJsonObject counts{{"total", materials_.size()}, {"succeeded", completed_.size()}, {"failed", failed_},
            {"pending", qMax(0, materials_.size() - completed_.size() - failed_ - unknown_.size())},
            {"cancelled", 0}, {"skipped", 0}, {"unknown", unknown_.size()}};
        done({QJsonObject{{"accountRef", accountRef_}, {"completedItemIds", completed_}, {"unknownItemIds", unknown_},
            {"itemIds", QJsonArray::fromStringList(request_.itemIds)}, {"counts", counts}, {"receipts", receipts_}}, error, blocked});
        deleteLater();
    }
    void next() {
        if (!done_ || cancelled_->load()) { finish(failure("job.cancelled")); return; }
        ++index_;
        attempt_ = 0;
        frozen_ = {};
        if (index_ == materials_.size()) { finish(); return; }
        emitItem("running", "validate");
        const auto material = materials_[index_];
        const auto cancelled = cancelled_;
        validation_.setFuture(QtConcurrent::run([material, cancelled] {
            FrozenUpload result;
            result.directory = std::make_shared<QTemporaryDir>();
            if (!result.directory->isValid()) return result;
            result.files = material.files;
            const auto metadata = material.descriptor.value("files").toArray();
            const auto paths = files(material.files);
            for (int i = 0; i < paths.size(); ++i) {
                const QFileInfo file(paths[i].second);
                const auto expected = metadata[i].toObject();
                if (!file.isFile() || QString::number(file.size()) != expected.value("bytes").toString()
                    || file.lastModified().toUTC().toString(Qt::ISODateWithMs) != expected.value("modifiedAt").toString()) return result;
                QFile source(file.absoluteFilePath());
                const auto destination = QDir(result.directory->path()).filePath(file.fileName());
                QSaveFile target(destination);
                if (!source.open(QIODevice::ReadOnly) || !target.open(QIODevice::WriteOnly)) return result;
                QCryptographicHash hash(QCryptographicHash::Sha256);
                while (!source.atEnd()) {
                    if (cancelled->load()) return result;
                    const auto bytes = source.read(64 * 1024);
                    if (source.error() != QFileDevice::NoError || target.write(bytes) != bytes.size()) return result;
                    hash.addData(bytes);
                }
                if (QString::fromLatin1(hash.result().toHex()) != expected.value("sha256").toString() || !target.commit()) return result;
                const auto kind = paths[i].first;
                if (kind == QLatin1String("chart")) result.files.chartPath = destination;
                else if (kind == QLatin1String("image")) result.files.backgroundPath = destination;
                else if (kind == QLatin1String("track")) result.files.trackPath = destination;
                else result.files.videoPath = destination;
            }
            result.valid = true;
            return result;
        }));
        connect(&validation_, &QFutureWatcher<FrozenUpload>::finished, this, [this] {
            disconnect(&validation_, nullptr, this, nullptr);
            if (cancelled_->load()) { finish(failure("job.cancelled")); return; }
            frozen_ = validation_.result();
            if (!frozen_.valid) { emitItem("failed", "validate", failure("file.changed")); ++failed_; QTimer::singleShot(0, this, [this] { next(); }); return; }
            send();
        });
    }
    void send() {
        if (!done_ || cancelled_->load()) return;
        const auto wait = qMax(provider_.accountRetryAt_, provider_.uploadReadyAt_) - provider_.clock_.elapsed();
        if (wait > 3600000) { emitItem("blocked", "upload", failure("upstream.rate_limited")); finish(failure("upstream.rate_limited"), true); return; }
        if (wait > 0) {
            emitItem("retry_wait", attempt_ > 0 ? "upload_retry" : "upload_spacing", {},
                {{"retryAt", QDateTime::currentDateTimeUtc().addMSecs(wait).toString(Qt::ISODateWithMs)}});
            QTimer::singleShot(int(wait), this, [this] { send(); }); return;
        }
        if (!provider_.accounts_.contains(accountRef_)) { finish(failure("upstream.auth_required")); return; }
        if (!log("send")) { emitItem("blocked", "upload", failure("diagnostic.unavailable")); finish(failure("diagnostic.unavailable"), true); return; }
        NetHttpRequest request;
        request.url = QUrl(provider_.baseUrl_.toString() + "/maichart/upload");
        request.method = "POST";
        request.accountRef = accountRef_;
        request.timeoutMs = 90000;
        request.requireNonEmpty = false;
        request.headers = {{"Referer", "https://majdata.net/user/charts"}};
        for (const auto& [kind, path] : files(frozen_.files)) {
            const QString name = kind == QLatin1String("chart") ? "maidata.txt" : kind == QLatin1String("track") ? "track.mp3"
                : kind == QLatin1String("video") ? "pv.mp4" : "bg." + QFileInfo(path).suffix().toLower();
            request.parts.append({"formfiles", {}, path, name, "application/octet-stream"});
        }
        emitItem("running", "upload");
        QPointer<NetUploadOperation> self(this);
        cancelHttp_ = provider_.transport_.request(request, [self](NetHttpResponse response) { if (self && self->done_) self->receive(std::move(response)); },
            [self](qint64 received, qint64 total) {
                if (self && self->done_) self->emitItem("running", "upload", {}, {{"bytesReceived", QString::number(received)},
                    {"bytesTotal", total >= 0 ? QJsonValue(QString::number(total)) : QJsonValue::Null}, {"attempt", self->attempt_ + 1}});
            });
    }
    void receive(NetHttpResponse response) {
        cancelHttp_ = {};
        provider_.uploadReadyAt_ = provider_.clock_.elapsed() + 5000;
        const auto code = response.error.value("code").toString();
        const bool logged = log("response", response);
        if ((response.sent && response.status == 0) || (response.sent && (code == QLatin1String("upstream.timeout") || code == QLatin1String("upstream.network") || response.cancelled))) {
            emitItem("outcome_unknown", "upload", failure("upload.outcome_unknown"));
            unknown_.append(request_.itemIds[index_]);
            finish(failure("upload.outcome_unknown")); return;
        }
        if (cancelled_->load()) { finish(failure("job.cancelled")); return; }
        if (!logged) {
            if (response.error.isEmpty() && !assessment(response).isApplicationError && !assessment(response).isHtml) {
                emitItem("succeeded", "complete"); completed_.append(request_.itemIds[index_]);
                receipts_.append(QJsonObject{{"itemId", request_.itemIds[index_]}, {"status", response.status}});
            } else { emitItem("failed", "upload", response.error.isEmpty() ? failure("upstream.validation") : response.error); ++failed_; }
            finish(failure("diagnostic.unavailable")); return;
        }
        if (code == QLatin1String("upstream.rate_limited") && attempt_++ == 0) {
            const auto delay = retryDelay(response);
            provider_.accountRetryAt_ = provider_.clock_.elapsed() + delay;
            if (delay > 3600000) { emitItem("blocked", "upload", response.error); finish(response.error, true); return; }
            emitItem("retry_wait", "upload_retry", response.error,
                {{"retryAt", QDateTime::currentDateTimeUtc().addMSecs(delay).toString(Qt::ISODateWithMs)}});
            QTimer::singleShot(int(delay), this, [this] { send(); }); return;
        }
        if (code == QLatin1String("upstream.blocked") || code == QLatin1String("upstream.rate_limited")) {
            emitItem("blocked", "upload", response.error);
            finish(response.error, true); return;
        }
        const auto parsed = assessment(response);
        if (!response.error.isEmpty() || parsed.isApplicationError || parsed.isHtml) {
            const auto error = response.error.isEmpty() ? failure(parsed.isHtml ? "upstream.invalid_payload" : "upstream.validation") : response.error;
            emitItem("failed", "upload", error); ++failed_;
            if (code == QLatin1String("upstream.auth_required")) { provider_.accounts_.remove(accountRef_); provider_.transport_.releaseAccount(accountRef_); finish(error); return; }
        } else {
            emitItem("succeeded", "complete");
            completed_.append(request_.itemIds[index_]);
            receipts_.append(QJsonObject{{"itemId", request_.itemIds[index_]}, {"status", response.status}});
        }
        QTimer::singleShot(0, this, [this] { next(); });
    }
    NetProvider& provider_;
    NetTaskRequest request_;
    NetEnginePort::Event event_;
    NetEnginePort::Done done_;
    std::shared_ptr<std::atomic_bool> cancelled_;
    NetTransportPort::Cancel cancelHttp_;
    QFutureWatcher<FrozenUpload> validation_;
    FrozenUpload frozen_;
    QList<NetProvider::UploadMaterial> materials_;
    QString accountRef_;
    QJsonArray completed_, unknown_, receipts_;
    int index_ = -1, attempt_ = 0, failed_ = 0;
    bool ownsTransfer_ = false;
};

NetEnginePort::Cancel NetProvider::executeUploadOperation(NetTaskRequest request, Event event, Done done) {
    if (request.operation == QLatin1String("net.accounts.login")) {
        auto* operation = new NetAccountOperation(*this, std::move(request), std::move(event), std::move(done));
        QTimer::singleShot(0, operation, [operation] { operation->start(); });
        return [weak = QPointer<NetAccountOperation>(operation)] { if (weak) weak->cancel(); };
    }
    if (request.operation == QLatin1String("net.uploads.scan")) {
        auto* operation = new NetScanOperation(*this, std::move(request), std::move(done));
        QTimer::singleShot(0, operation, [operation] { operation->start(); });
        return [weak = QPointer<NetScanOperation>(operation)] { if (weak) weak->cancel(); };
    }
    auto* operation = new NetUploadOperation(*this, std::move(request), std::move(event), std::move(done));
    QTimer::singleShot(0, operation, [operation] { operation->start(); });
    return [weak = QPointer<NetUploadOperation>(operation)] { if (weak) weak->cancel(); };
}
NetCallResult NetProvider::accountCall(const QString& principal, const QString& operation, const QJsonObject& parameters) {
    if (operation == QLatin1String("net.accounts.list")) {
        QJsonArray accounts;
        for (const auto& account : accounts_) if (account.principal == principal) accounts.append(account.descriptor);
        return {accounts, {}};
    }
    const QString accountRef = parameters.value("accountRef").toString();
    if (accounts_.value(accountRef).principal != principal) return {{}, failure("permission.denied")};
    const auto cancel = accountTransfers_.value(accountRef);
    accounts_.remove(accountRef);
    if (cancel) cancel();
    transport_.releaseAccount(accountRef);
    return {QJsonObject{{"released", true}}, {}};
}
QString NetProvider::uploadMaterialPath(const QString& principal, const QString& planRef, const QString& itemId) const {
    const auto plan = uploadPlans_.constFind(planRef);
    if (plan == uploadPlans_.cend() || plan->principal != principal || plan->expiresAt <= clock_.elapsed()) return {};
    for (const auto& material : plan->materials) {
        if (material.descriptor.value("itemId") == itemId) return material.files.directoryPath;
    }
    return {};
}
void NetProvider::setAccountRemembered(const QString& principal, const QString& accountRef, bool remembered) {
    const auto account = accounts_.find(accountRef);
    if (account != accounts_.end() && account->principal == principal) account->descriptor.insert("remembered", remembered);
}
} // namespace miacode::net
