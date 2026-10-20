#include "app/services/net/NetService.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QTextStream>
#include <QTimer>

namespace {
bool check(bool condition, const char* message)
{
    if (!condition) QTextStream(stderr) << "FAIL: " << message << '\n';
    return condition;
}
class Engine final : public miacode::net::NetEnginePort {
public:
    int calls = 0;
    int cancellations = 0;
    miacode::net::NetTaskRequest request;
    Event event;
    Done done;
    bool immediate = false;
    bool supports(const QString&) const override { return true; }
    Cancel execute(miacode::net::NetTaskRequest value, Event report, Done callback) override
    {
        ++calls;
        request = std::move(value);
        event = std::move(report);
        done = std::move(callback);
        if (immediate) {
            auto completion = std::move(done);
            completion({QJsonObject{{"classification", "normal"}, {"elapsedMs", 10}, {"upstreamStatus", 200}}, {}});
        }
        return [this] {
            ++cancellations;
            if (done) {
                auto completion = std::move(done);
                completion({{}, miacode::jobError(QStringLiteral("job.cancelled"))});
            }
        };
    }
    miacode::net::NetCallResult call(const QString&, const QString&, const QJsonObject&) override { return {}; }
};
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    using namespace miacode;
    JobRegistry jobs;
    Engine engine;
    NetService service(jobs, engine);
    const QString principal = QStringLiteral("desktop");
    const auto cancelled = service.start(principal, QStringLiteral("net.queries.create"), {{"providerId", "majdata"}, {"title", "song"}});
    const auto cancelledId = cancelled.result.toObject().value("jobId").toString();
    jobs.cancel(cancelledId, principal);
    QCoreApplication::processEvents();
    bool ok = check(engine.calls == 0 && jobs.snapshot(cancelledId, principal).value("state") == "cancelled",
        "cancel before launch sends no network request");
    const QJsonObject parameters{{"providerId", "majdata"}, {"chartIds", QJsonArray{"a", "b"}}, {"destination", QJsonObject{{"kind", "managed"}}}};
    ok &= check(!service.start(principal, QStringLiteral("net.downloads.create"), parameters).ok(),
        "download requires an idempotency key");
    const auto first = service.start(principal, QStringLiteral("net.downloads.create"), parameters, QStringLiteral("key"));
    const auto jobId = first.result.toObject().value("jobId").toString();
    const auto repeated = service.start(principal, QStringLiteral("net.downloads.create"), parameters, QStringLiteral("key"));
    ok &= check(first.ok() && repeated.result.toObject().value("jobId") == jobId, "same request key reuses one job");
    auto different = parameters;
    different.insert("chartIds", QJsonArray{"other"});
    ok &= check(service.start(principal, QStringLiteral("net.downloads.create"), different, QStringLiteral("key"))
        .error.value("code") == "idempotency.conflict", "same key with changed input conflicts");
    QCoreApplication::processEvents();
    ok &= check(engine.calls == 1 && engine.request.itemIds.size() == 2, "engine gets one frozen task and stable item IDs");
    engine.event({0, QStringLiteral("succeeded"), QStringLiteral("downloaded"), {}, {}, {}});
    engine.event({1, QStringLiteral("running"), QStringLiteral("download"), {}, {}, {}});
    jobs.cancel(jobId, principal);
    const auto cancelledSnapshot = jobs.snapshot(jobId, principal);
    ok &= check(engine.cancellations == 1 && cancelledSnapshot.value("state") == "cancelled"
        && cancelledSnapshot.value("counts").toObject().value("succeeded") == 1,
        "cancel reaches task owner and preserves published item");

    const auto blocked = service.start(principal, QStringLiteral("net.downloads.create"), parameters, QStringLiteral("blocked"));
    const auto blockedId = blocked.result.toObject().value("jobId").toString();
    QCoreApplication::processEvents();
    engine.event({0, QStringLiteral("succeeded"), QStringLiteral("downloaded"), {}, {}, {}});
    engine.event({1, QStringLiteral("blocked"), QStringLiteral("challenge"), {}, {}, {}});
    auto completion = std::move(engine.done);
    completion({{}, jobError(QStringLiteral("upstream.blocked")), true});
    const auto version = jobs.snapshot(blockedId, principal).value("version");
    const auto resumed = service.call(principal, QStringLiteral("jobs.resume"),
        {{"jobId", blockedId}, {"expectedJobVersion", version}, {"reason", "verified"}});
    ok &= check(resumed.ok() && engine.request.parameters.value("pendingIndices").toArray() == QJsonArray{1},
        "resume excludes completed inputs");
    jobs.cancel(blockedId, principal);
    engine.immediate = true;
    const auto synchronous = service.start(principal, QStringLiteral("net.probes.create"), {{"providerId", "majdata"}});
    QCoreApplication::processEvents();
    const auto synchronousId = synchronous.result.toObject().value("jobId").toString();
    const int before = engine.cancellations;
    jobs.cancel(synchronousId, principal);
    ok &= check(jobs.snapshot(synchronousId, principal).value("state") == "succeeded"
        && engine.cancellations == before, "synchronous adapter completion does not retain obsolete cancel handler");
    const auto login = service.start(principal, QStringLiteral("net.accounts.login"),
        {{"providerId", "majdata"}, {"username", "account"}, {"password", "password-secret"}}, QStringLiteral("login-key"));
    QCoreApplication::processEvents();
    ok &= check(login.ok() && !QJsonDocument(jobs.list(principal)).toJson().contains("password-secret"),
        "login request secret is absent from registry and events");
    return ok ? 0 : 1;
}
