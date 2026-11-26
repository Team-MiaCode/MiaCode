#include "media_tools/net/NetProvider.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QTextStream>
#include <QTimer>
#include <QUrlQuery>

#include <memory>

namespace {
bool check(bool condition, const char* message)
{
    if (!condition) QTextStream(stderr) << "FAIL: " << message << '\n';
    return condition;
}
class Transport final : public QObject, public miacode::net::NetTransportPort {
public:
    QList<miacode::net::NetHttpRequest> requests;
    QList<miacode::net::NetHttpResponse> responses;
    Cancel request(const miacode::net::NetHttpRequest& request, Done done, Progress = {}) override
    {
        requests.append(request);
        auto response = responses.isEmpty() ? miacode::net::NetHttpResponse{} : responses.takeFirst();
        auto cancelled = std::make_shared<bool>(false);
        QTimer::singleShot(0, this, [response, done = std::move(done), cancelled]() mutable {
            if (*cancelled) {
                response.cancelled = true;
                response.error = {{"code", "job.cancelled"}};
            }
            done(std::move(response));
        });
        return [cancelled] { *cancelled = true; };
    }
    void releaseAccount(const QString&) override {}
};

miacode::net::NetHttpResponse json(const QByteArray& payload, int elapsed = 10)
{
    miacode::net::NetHttpResponse result;
    result.payload = payload;
    result.status = 200;
    result.elapsedMs = elapsed;
    return result;
}
miacode::net::NetTaskResult run(miacode::net::NetProvider& provider,
                               miacode::net::NetTaskRequest request, bool cancel = false)
{
    QEventLoop loop;
    miacode::net::NetTaskResult result;
    bool completed = false;
    auto stop = provider.execute(std::move(request), {}, [&](auto value) {
        result = std::move(value);
        completed = true;
        loop.quit();
    });
    if (cancel) stop();
    if (!completed) {
        QTimer::singleShot(3000, &loop, [&] { stop(); loop.quit(); });
        loop.exec();
    }
    return result;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    using namespace miacode::net;
    Transport transport;
    NetProvider provider(transport);
    const QByteArray fixture = R"([
      {"id":"a","title":"Song A","uploader":"alice","publicTags":["event"],"levels":["13+"],"timestamp":"2026-10-09T00:00:00Z"},
      {"id":"b","title":"Song B","uploader":"ALICE","tag":"event","levels":["13"],"timestamp":"2026-10-09T01:00:00Z"},
      {"id":"c","title":"Other","uploader":"alice","tag":"event","timestamp":"2026-10-09T00:00:00Z"},
      {"id":"d","title":"Song D","uploader":"bob","tag":"event","timestamp":"2026-10-09T00:00:00Z"},
      {"id":"bad","timestamp":"invalid"}
    ])";
    transport.responses.append(json(fixture));
    NetTaskRequest request{QStringLiteral("owner"), QStringLiteral("net.queries.create"),
        {{"providerId", "majdata"}, {"uploader", "Alice"}, {"tag", "TAG:event"}, {"title", "song"},
         {"sort", "level_desc"}}, {}};
    const auto query = run(provider, request);
    bool ok = check(query.error.isEmpty() && query.result.toObject().value("matchedCount") == 2
        && query.result.toObject().value("skippedRows") == 1, "provider reports final AND result and skipped rows");
    ok &= check(transport.requests.size() == 1
        && QUrlQuery(transport.requests.first().url).queryItemValue(QStringLiteral("search")) == QStringLiteral("uploader:Alice"),
        "combined search uses uploader candidates without extra title fetch");
    const auto queryRef = query.result.toObject().value("queryRef").toString();
    auto page = provider.call(QStringLiteral("owner"), QStringLiteral("net.queries.results"),
        {{"queryRef", queryRef}, {"limit", 1}});
    ok &= check(page.ok() && page.result.toObject().value("items").toArray().first().toObject().value("chartId") == "a",
        "first page applies stable sorting");
    const auto cursor = page.result.toObject().value("nextCursor").toString();
    page = provider.call(QStringLiteral("owner"), QStringLiteral("net.queries.results"),
        {{"queryRef", queryRef}, {"limit", 1}, {"cursor", cursor}});
    ok &= check(page.ok() && page.result.toObject().value("items").toArray().first().toObject().value("chartId") == "b"
        && page.result.toObject().value("nextCursor").isNull(), "second page has no duplicate or missing rows");
    ok &= check(!provider.call(QStringLiteral("other"), QStringLiteral("net.queries.results"), {{"queryRef", queryRef}}).ok(),
        "query snapshots are scoped to their principal");
    request.parameters.insert("title", "other");
    const auto cached = run(provider, request);
    ok &= check(cached.result.toObject().value("matchedCount") == 1
        && cached.result.toObject().value("skippedRows") == 1 && transport.requests.size() == 1,
        "candidate cache preserves parse warnings and reapplies filters");
    request.parameters.insert("forceRefresh", true);
    transport.responses.append(json(fixture));
    run(provider, request);
    ok &= check(transport.requests.size() == 2, "force refresh makes a new request");
    request.parameters.insert("startDate", "not-a-date");
    const auto invalid = run(provider, request);
    ok &= check(invalid.error.value("code") == "request.invalid", "invalid supplied date is rejected before transport");
    request.parameters.remove("startDate");
    transport.responses.append(json("<html>failed</html>"));
    const auto corrupt = run(provider, request);
    ok &= check(corrupt.error.value("code") == "upstream.invalid_payload", "invalid list is not an empty successful result");
    const NetTaskRequest probeRequest{"owner", "net.probes.create", {{"providerId", "majdata"}}, {}};
    const auto requestsBeforeProbe = transport.requests.size();
    NetHttpResponse timeout;
    timeout.error = {{"code", "upstream.timeout"}};
    transport.responses.append(timeout);
    transport.responses.append(json("[]", 1000));
    const auto probe = run(provider, probeRequest);
    ok &= check(probe.result.toObject().value("classification") == "slow"
        && transport.requests.size() == requestsBeforeProbe + 2,
        "probe recovers from one timeout and keeps the inclusive slow threshold");
    ok &= check(!transport.requests[requestsBeforeProbe].freshConnection && transport.requests.last().freshConnection
        && transport.requests.last().timeoutMs == 8000
        && QUrlQuery(transport.requests.last().url).queryItemValue("page") == "0",
        "probe limits upstream results and retries on a fresh connection");
    transport.responses.append(timeout);
    transport.responses.append(timeout);
    const auto failedProbe = run(provider, probeRequest);
    ok &= check(failedProbe.error.value("code") == "upstream.timeout"
        && transport.requests.size() == requestsBeforeProbe + 4, "persistent probe timeouts are reported after one retry");
    const auto cancelled = run(provider, request, true);
    ok &= check(cancelled.error.value("code") == "job.cancelled", "queued provider operation can be cancelled before transport");

    const QByteArray keywordFixture = R"([
      {"id":"e","title":"First","artist":"Artist + One","uploader":"alice","timestamp":"2026-10-09T00:00:00Z"},
      {"id":"f","title":"Second","designer":"Artist + One","uploader":"bob","timestamp":"2026-10-09T00:00:00Z"}
    ])";
    const QJsonObject keyword{{"providerId", "majdata"}, {"search", "Artist +"}, {"uploader", "alice"}};
    transport.responses.append(json(keywordFixture));
    const auto suggestion = run(provider, {"owner", "net.queries.suggest", keyword, {}});
    const auto suggestions = suggestion.result.toArray();
    ok &= check(suggestion.error.isEmpty() && suggestions.size() == 1
        && suggestions.first().toObject().value("kind") == "artist"
        && transport.requests.last().url.query(QUrl::FullyEncoded).contains("%2B")
        && QUrlQuery(transport.requests.last().url).queryItemValue("page") == "0",
        "autocomplete uses the keyword, preserves plus signs and applies selected filters");
    transport.responses.append(json(keywordFixture));
    const auto keywordResult = run(provider, {"owner", "net.queries.create", keyword, {}});
    ok &= check(keywordResult.error.isEmpty() && keywordResult.result.toObject().value("matchedCount") == 1
        && !QUrlQuery(transport.requests.last().url).hasQueryItem("page"),
        "full keyword results are independent of the suggestion page and preserve non-title matches");
    return ok ? 0 : 1;
}
