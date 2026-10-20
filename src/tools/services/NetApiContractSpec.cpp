#include "app/services/api/ApiCatalog.h"
#include "app/services/api/ApiDispatcher.h"
#include "media_tools/net/NetProvider.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QJsonDocument>
#include <QTextStream>
#include <QTimer>

namespace {
bool check(bool condition, const char* message)
{
    if (!condition) QTextStream(stderr) << "FAIL: " << message << '\n';
    return condition;
}
class Transport final : public miacode::net::NetTransportPort {
public:
    int calls = 0;
    Cancel request(const miacode::net::NetHttpRequest&, Done done, Progress) override
    {
        ++calls;
        QTimer::singleShot(0, [done] {
            miacode::net::NetHttpResponse response;
            response.status = 200;
            response.payload = R"([{"id":"a","title":"Song","uploader":"Alice","timestamp":"2026-10-01T12:00:00Z"}])";
            done(response);
        });
        return {};
    }
    void releaseAccount(const QString&) override {}
};
QString code(const miacode::api::ApiReply& reply)
{
    return reply.envelope.value(QStringLiteral("error")).toObject().value(QStringLiteral("code")).toString();
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    using namespace miacode;
    using namespace miacode::api;
    bool ok = check(ApiCatalog::operations().size() == 34, "all agreed operations are registered");
    const auto normalized = ApiCatalog::validate(QStringLiteral("net.queries.create"),
        {{"providerId", "majdata"}, {"uploader", " Alice "}, {"tag", " TaG: event "}, {"startDate", "2026-10-09"}, {"endDate", "2026-10-01"}});
    ok &= check(normalized.ok() && normalized.parameters.value("uploader") == "Alice"
        && normalized.parameters.value("tag") == "event" && normalized.parameters.value("providerId") == "majdata"
        && normalized.parameters.value("startDate") == "2026-10-01" && normalized.parameters.value("caseSensitive") == false,
        "shared defaults, trim, tag normalization and reverse date range");
    for (const QJsonObject& bad : {QJsonObject{{"title", "song"}, {"sort", "unknown"}},
        QJsonObject{{"title", "song"}, {"caseSensitive", "false"}}, QJsonObject{{"title", "song"}, {"secret", "hidden"}},
        QJsonObject{{"title", "song"}, {"startDate", "2026-02-30"}, {"endDate", "2026-03-01"}},
        QJsonObject{{"title", "song"}, {"startDate", "2026-10-01"}}, QJsonObject{{"tag", "tag: "}},
        QJsonObject{{"title", "song"}, {"timeZone", "Not/AZone"}}, QJsonObject{{"title", QJsonValue::Null}}}) {
        auto request = bad;
        request.insert(QStringLiteral("providerId"), QStringLiteral("majdata"));
        ok &= check(!ApiCatalog::validate(QStringLiteral("net.queries.create"), request).ok(), "invalid query is rejected before dispatch");
    }
    ok &= check(!ApiCatalog::validate(QStringLiteral("net.downloads.create"),
        {{"providerId", "majdata"}, {"chartIds", QJsonArray{"a", "a"}}, {"destination", QJsonObject{{"kind", "managed"}}}}).ok(), "duplicate batch references are rejected");
    ok &= check(!ApiCatalog::validate(QStringLiteral("net.accounts.login"), {{"providerId", "majdata"}, {"username", " "}, {"password", "secret"}}).ok(), "blank normalized username is rejected");
    ok &= check(!ApiCatalog::validate(QStringLiteral("net.queries.create"), {{"title", "Song"}}).ok(), "required fields cannot be synthesized by defaults");
    ok &= check(!ApiCatalog::validate(QStringLiteral("jobs.events"), {{"jobId", "job_a"}, {"after", "18446744073709551616"}}).ok()
        && !ApiCatalog::validate(QStringLiteral("jobs.events"), {{"jobId", "job_a"}, {"after", 1}}).ok(), "uint64 overflow and JS numbers rejected");
    ok &= check(!ApiCatalog::validate(QStringLiteral("network.http.fetch"), {{"url", "https://user:secret@example.com/a"}, {"targetGrantRef", "target_a"}}).ok()
        && !ApiCatalog::validate(QStringLiteral("network.http.fetch"), {{"url", "file:///tmp/data"}, {"targetGrantRef", "target_a"}}).ok(), "generic URLs reject embedded credentials and unsupported protocols");
    ok &= check(!ApiCatalog::validate(QStringLiteral("network.proxy.set"), {{"profileId", "net"}, {"mode", "socks5"}}).ok()
        && !ApiCatalog::validate(QStringLiteral("network.proxy.set"), {{"profileId", "net"}, {"mode", "direct"}, {"host", "proxy"}}).ok(), "conditional proxy rules enforced");

    Transport transport;
    net::NetProvider provider(transport);
    JobRegistry jobs;
    NetService netService(jobs, provider);
    ApiDispatcher api(netService, QStringLiteral("test"));
    ApiContext context{QStringLiteral("client_a"), {QStringLiteral("api.read"), QStringLiteral("net.read"),
        QStringLiteral("jobs.read"), QStringLiteral("jobs.control"), QStringLiteral("net.download"), QStringLiteral("files.write")}, {}, {}};
    ok &= check(api.dispatch({}, QStringLiteral("api.capabilities")).status == 401, "authentication precedes execution");
    auto unauthorized = context;
    unauthorized.scopes.clear();
    ok &= check(api.dispatch(unauthorized, QStringLiteral("net.queries.create"), {{"title", "Song"}}).status == 403 && transport.calls == 0,
        "missing scope is rejected without touching transport");
    const auto caps = api.dispatch(context, QStringLiteral("api.capabilities"));
    ok &= check(caps.status == 200 && caps.envelope.value("ok").toBool(), "capability result matches schema");
    int available = 0;
    for (const auto& value : caps.envelope.value("result").toObject().value("operations").toArray()) {
        const auto operation = value.toObject();
        available += operation.value("available").toBool() ? 1 : 0;
        ok &= check(!operation.value("adapters").toObject().value("http").toBool(), "planned HTTP routes are not claimed delivered");
    }
    ok &= check(available == 11, "runtime reports precisely the authorized internal callable set");
    ok &= check(api.dispatch(context, QStringLiteral("net.providers.list")).status == 200, "provider descriptor conforms to shared DTO");
    ok &= check(code(api.dispatch(context, QStringLiteral("net.downloads.create"),
        {{"providerId", "majdata"}, {"chartIds", QJsonArray{"a"}}, {"destination", QJsonObject{{"kind", "managed"}}}})) == QLatin1String("request.invalid")
        && transport.calls == 0, "download requires an idempotency key before starting a transfer");
    auto settingsContext = context;
    settingsContext.scopes.insert(QStringLiteral("net.settings.read"));
    ok &= check(code(api.dispatch(settingsContext, QStringLiteral("net.settings.get"))) == QLatin1String("capability.unavailable"),
        "registered settings operation reports its planned capability");
    const auto accepted = api.dispatch(context, QStringLiteral("net.queries.create"), {{"providerId", "majdata"}, {"title", "Song"}});
    const QString jobId = accepted.envelope.value("result").toObject().value("jobId").toString();
    ok &= check(accepted.status == 202 && !jobId.isEmpty(), "query returns a task handle");
    QEventLoop loop;
    QObject::connect(&jobs, &JobRegistry::changed, &loop, [&](const QString& id) {
        if (id == jobId && JobRegistry::isTerminal(jobs.snapshot(id, context.principal).value("state").toString())) loop.quit();
    });
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    loop.exec();
    const auto snapshot = api.dispatch(context, QStringLiteral("jobs.get"), {{"jobId", jobId}});
    ok &= check(snapshot.status == 200 && snapshot.envelope.value("result").toObject().value("state") == "succeeded", "query task finishes through provider");
    const QString queryRef = snapshot.envelope.value("result").toObject().value("result").toObject().value("queryRef").toString();
    const auto page = api.dispatch(context, QStringLiteral("net.queries.results"), {{"queryRef", queryRef}});
    ok &= check(page.status == 200 && page.envelope.value("result").toObject().value("items").toArray().size() == 1, "query result DTO is checked at public boundary");
    auto other = context;
    other.principal = QStringLiteral("client_b");
    for (const auto& operation : {QStringLiteral("jobs.get"), QStringLiteral("jobs.items"), QStringLiteral("jobs.events"), QStringLiteral("jobs.cancel")})
        ok &= check(api.dispatch(other, operation, {{"jobId", jobId}}).status == 403, "job read/control cannot cross principals");
    ok &= check(api.dispatch(other, QStringLiteral("net.queries.results"), {{"queryRef", queryRef}}).status == 403, "query snapshot cannot cross principals");
    ok &= check(api.dispatch(context, QStringLiteral("jobs.events"), {{"jobId", jobId}}).status == 200, "event page conforms to schema");
    const auto token = jobs.create(context.principal, QStringLiteral("net.download"), QJsonArray{QJsonObject{{"chartRef", "a"}}, QJsonObject{{"chartRef", "b"}}});
    const auto items = api.dispatch(context, QStringLiteral("jobs.items"), {{"jobId", token.jobId}, {"limit", 1}}).envelope.value("result").toObject();
    const QString cursor = items.value("nextCursor").toString();
    jobs.setState(token, QStringLiteral("running"), QStringLiteral("download"));
    jobs.updateItem(token, jobs.itemIds(token)[1], QStringLiteral("succeeded"), QStringLiteral("published"));
    const auto next = api.dispatch(context, QStringLiteral("jobs.items"), {{"jobId", token.jobId}, {"limit", 1}, {"cursor", cursor}});
    ok &= check(next.status == 200 && next.envelope.value("result").toObject().value("items").toArray()[0].toObject().value("state") == "pending",
        "task item pagination retains the first page snapshot across progress changes");
    ok &= check(api.dispatch(other, QStringLiteral("jobs.items"), {{"jobId", token.jobId}, {"cursor", cursor}}).status == 403, "page cursor is principal owned");
    ok &= check(api.dispatch(context, QStringLiteral("jobs.items"), {{"jobId", jobId}, {"cursor", cursor}}).status == 410, "page cursor is bound to the task");
    const auto itemEvents = api.dispatch(context, QStringLiteral("jobs.events"), {{"jobId", token.jobId}});
    const auto event = itemEvents.envelope.value("result").toObject().value("events").toArray().last().toObject();
    ok &= check(itemEvents.status == 200 && event.value("item").toObject().value("state") == "succeeded", "item event supplies structured item state");
    return ok ? 0 : 1;
}
