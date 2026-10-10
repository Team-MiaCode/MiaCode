#include "media_tools/net/NetProvider.h"

#include "media_tools/net/NetClient.h"

#include <QJsonDocument>
#include <QPointer>
#include <QTimer>
#include <QUuid>
#include <QUrlQuery>
#include <QStandardPaths>
#include <QDir>

namespace miacode::net {
namespace {
QJsonObject failure(const QString& code)
{
    return {{QStringLiteral("code"), code}, {QStringLiteral("message"), code},
            {QStringLiteral("retryable"), code == QLatin1String("upstream.network")}};
}
QString reference(const char* prefix)
{
    return QString::fromLatin1(prefix) + QUuid::createUuid().toString(QUuid::Id128);
}
QJsonObject summary(const NetChartSummary& chart)
{
    return {{QStringLiteral("providerId"), QStringLiteral("majdata")}, {QStringLiteral("chartId"), chart.id},
        {QStringLiteral("title"), chart.title}, {QStringLiteral("artist"), chart.artist},
        {QStringLiteral("designer"), chart.designer}, {QStringLiteral("uploader"), chart.uploader},
        {QStringLiteral("levels"), QJsonArray::fromStringList(chart.levels)},
        {QStringLiteral("tags"), QJsonArray::fromStringList(chart.publicTags)},
        {QStringLiteral("uploadedAtUtc"), chart.timestampUtc.toUTC().toString(Qt::ISODateWithMs)},
        {QStringLiteral("remoteVersion"), chart.hash.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(chart.hash)}};
}
}

class NetQueryOperation final : public QObject {
public:
    NetQueryOperation(NetProvider& provider, NetTaskRequest task, NetEnginePort::Event event,
                      NetEnginePort::Done done)
        : QObject(&provider), provider_(provider), task_(std::move(task)), event_(std::move(event)), done_(std::move(done))
    {
        const auto& values = task_.parameters;
        request_.providerId = values.value(QStringLiteral("providerId")).toString(QStringLiteral("majdata"));
        request_.uploader = values.value(QStringLiteral("uploader")).toString();
        request_.tag = values.value(QStringLiteral("tag")).toString();
        request_.title = values.value(QStringLiteral("title")).toString();
        request_.caseSensitive = values.value(QStringLiteral("caseSensitive")).toBool(false);
        request_.forceRefresh = values.value(QStringLiteral("forceRefresh")).toBool(false);
        request_.sort = values.value(QStringLiteral("sort")).toString(QStringLiteral("uploaded_desc"));
        const QString zone = values.value(QStringLiteral("timeZone")).toString();
        if (!zone.isEmpty()) request_.timeZone = QTimeZone(zone.toUtf8());
        if (values.contains(QStringLiteral("startDate"))) request_.startDate = QDate::fromString(values.value(QStringLiteral("startDate")).toString(), Qt::ISODate);
        if (values.contains(QStringLiteral("endDate"))) request_.endDate = QDate::fromString(values.value(QStringLiteral("endDate")).toString(), Qt::ISODate);
    }
    ~NetQueryOperation() override
    {
        done_ = {};
        if (cancelHttp_) cancelHttp_();
    }
    bool active() const { return static_cast<bool>(done_); }
    void start()
    {
        if (!active()) return;
        const auto& parameters = task_.parameters;
        const bool probe = task_.operation == QLatin1String("net.probes.create");
        QString invalid = probe ? QString{} : validateNetQuery(request_);
        if (request_.providerId != QLatin1String("majdata")) invalid = QStringLiteral("capability.unavailable");
        if (!probe && ((parameters.contains(QStringLiteral("startDate")) && !request_.startDate.isValid())
            || (parameters.contains(QStringLiteral("endDate")) && !request_.endDate.isValid()))) invalid = QStringLiteral("request.invalid");
        if (!invalid.isEmpty()) { finish({{}, failure(invalid)}); return; }
        if (!probe) {
            if (const auto cached = provider_.cache_.find(request_)) {
                skippedRows_ = cached->skippedRows;
                finishQuery(cached->charts);
                return;
            }
        }
        searches_ = probe ? QStringList{QStringLiteral("__miacode_connection_probe__")} : netCandidateSearches(request_);
        fetch();
    }
    void cancel()
    {
        if (!active()) return;
        cancelled_ = true;
        if (cancelHttp_) cancelHttp_();
        if (active()) finish({{}, failure(QStringLiteral("job.cancelled"))});
    }

private:
    void finish(NetTaskResult result)
    {
        if (!done_) return;
        auto done = std::move(done_);
        cancelHttp_ = {};
        QPointer<NetQueryOperation> self(this);
        done(std::move(result));
        if (self) self->deleteLater();
    }
    void finishQuery(const QList<NetChartSummary>& candidates)
    {
        const auto filtered = filterAndSortNetCharts(candidates, request_);
        finish({provider_.storeQuery(task_.principal, filtered, skippedRows_), {}});
    }
    void fetch()
    {
        if (!active() || cancelled_) return;
        NetHttpRequest http;
        http.url = QUrl(provider_.baseUrl_.toString() + QStringLiteral("/maichart/list"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("sort"), QString());
        query.addQueryItem(QStringLiteral("search"), searches_[searchIndex_]);
        http.url.setQuery(query);
        http.headers = {{QByteArrayLiteral("Referer"), request_.uploader.isEmpty()
            ? QByteArrayLiteral("https://majdata.net/") : netUserSpaceReferer(request_.uploader).toUtf8()}};
        http.timeoutMs = task_.operation == QLatin1String("net.probes.create") ? 8000 : 60000;
        QPointer<NetQueryOperation> self(this);
        cancelHttp_ = provider_.transport_.request(http, [self](NetHttpResponse response) {
            if (self && self->active()) self->receive(std::move(response));
        });
    }
    void receive(NetHttpResponse response)
    {
        cancelHttp_ = {};
        const bool probe = task_.operation == QLatin1String("net.probes.create");
        if (!response.error.isEmpty()) {
            const QString code = response.error.value(QStringLiteral("code")).toString();
            if (!probe && !cancelled_ && code == QLatin1String("upstream.network") && networkRetries_++ == 0) {
                if (event_) event_({-1, QStringLiteral("retry_wait"), QStringLiteral("query_retry"), {}, {}, {}});
                QTimer::singleShot(1000, this, [this] {
                    if (!active()) return;
                    if (event_) event_({-1, QStringLiteral("running"), QStringLiteral("query"), {}, {}, {}});
                    fetch();
                });
                return;
            }
            const bool blocked = code == QLatin1String("upstream.blocked") || code == QLatin1String("upstream.rate_limited");
            finish({{}, response.error, blocked && !probe});
            return;
        }
        QString parseError;
        const auto charts = parseChartListJson(response.payload, &parseError);
        if (!parseError.isEmpty()) { finish({{}, failure(QStringLiteral("upstream.invalid_payload"))}); return; }
        if (probe) {
            finish({QJsonObject{{QStringLiteral("classification"), response.elapsedMs >= 1000
                    ? QStringLiteral("slow") : QStringLiteral("normal")},
                {QStringLiteral("elapsedMs"), static_cast<int>(response.elapsedMs)},
                {QStringLiteral("upstreamStatus"), response.status}}, {}});
            return;
        }
        skippedRows_ += QJsonDocument::fromJson(response.payload).array().size() - charts.size();
        candidates_.append(charts);
        ++searchIndex_;
        networkRetries_ = 0;
        if (searchIndex_ < searches_.size()
            && (request_.uploader.trimmed().isEmpty() || candidates_.isEmpty())) {
            fetch();
            return;
        }
        provider_.cache_.insert(request_, candidates_, skippedRows_);
        finishQuery(candidates_);
    }
    NetProvider& provider_;
    NetTaskRequest task_;
    NetEnginePort::Event event_;
    NetEnginePort::Done done_;
    NetTransportPort::Cancel cancelHttp_;
    NetQueryRequest request_;
    QStringList searches_;
    QList<NetChartSummary> candidates_;
    int searchIndex_ = 0;
    int networkRetries_ = 0;
    int skippedRows_ = 0;
    bool cancelled_ = false;
};

NetProvider::NetProvider(NetTransportPort& transport, QObject* parent, QUrl baseUrl)
    : QObject(parent), transport_(transport), baseUrl_(std::move(baseUrl)), cache_([this] { return clock_.elapsed(); })
{
    clock_.start();
    const QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(cacheRoot);
    previewRoot_ = std::make_unique<QTemporaryDir>(QDir(cacheRoot).filePath(QStringLiteral("net-preview-XXXXXX")));
}

NetProvider::~NetProvider()
{
    // Operations borrow provider state, so release them before member destruction.
    const auto operations = children();
    for (auto* operation : operations) delete operation;
}

bool NetProvider::supports(const QString& operation) const
{
    return operation == QLatin1String("net.queries.create") || operation == QLatin1String("net.probes.create")
        || operation == QLatin1String("net.queries.results") || operation == QLatin1String("net.providers.list")
        || operation == QLatin1String("net.downloads.create") || operation == QLatin1String("net.previews.prepare")
        || operation == QLatin1String("net.previews.release") || operation == QLatin1String("net.uploads.scan")
        || operation == QLatin1String("net.uploads.create") || operation == QLatin1String("net.accounts.login")
        || operation == QLatin1String("net.accounts.list") || operation == QLatin1String("net.accounts.logout");
}

NetEnginePort::Cancel NetProvider::execute(NetTaskRequest request, Event event, Done done)
{
    if (request.operation == QLatin1String("net.accounts.login") || request.operation.startsWith("net.uploads."))
        return executeUploadOperation(std::move(request), std::move(event), std::move(done));
    if (request.operation == QLatin1String("net.downloads.create") || request.operation == QLatin1String("net.previews.prepare"))
        return executeResources(std::move(request), std::move(event), std::move(done));
    if (request.operation == QLatin1String("net.queries.create") || request.operation == QLatin1String("net.probes.create")) {
        auto* operation = new NetQueryOperation(*this, std::move(request), std::move(event), std::move(done));
        QTimer::singleShot(0, operation, [operation] { operation->start(); });
        QPointer<NetQueryOperation> weak(operation);
        return [weak] { if (weak) weak->cancel(); };
    }
    QTimer::singleShot(0, this, [done = std::move(done)] { done({{}, failure(QStringLiteral("capability.unavailable"))}); });
    return {};
}

QJsonObject NetProvider::storeQuery(const QString& principal, const QList<NetChartSummary>& charts, int skippedRows)
{
    const auto now = clock_.elapsed();
    for (auto it = queries_.begin(); it != queries_.end();) {
        if (it->expiresAt <= now) it = queries_.erase(it);
        else ++it;
    }
    for (auto it = cursors_.begin(); it != cursors_.end();) {
        if (!queries_.contains(it->queryRef)) it = cursors_.erase(it);
        else ++it;
    }
    if (queries_.size() >= 100) {
        queries_.remove(queries_.cbegin().key());
    }
    const QString queryRef = reference("query_");
    QJsonArray values;
    for (const auto& chart : charts) values.append(summary(chart));
    queries_.insert(queryRef, {principal, values, now + 900000});
    return {{QStringLiteral("queryRef"), queryRef}, {QStringLiteral("matchedCount"), values.size()},
        {QStringLiteral("skippedRows"), skippedRows}, {QStringLiteral("completeness"), QStringLiteral("unknown")},
        {QStringLiteral("expiresAt"), QDateTime::currentDateTimeUtc().addSecs(900).toString(Qt::ISODateWithMs)}};
}

NetCallResult NetProvider::queryPage(const QString& principal, const QJsonObject& parameters)
{
    const QString queryRef = parameters.value(QStringLiteral("queryRef")).toString();
    const auto found = queries_.constFind(queryRef);
    if (found == queries_.cend() || found->expiresAt <= clock_.elapsed()) return {{}, failure(QStringLiteral("query.expired"))};
    if (found->principal != principal) return {{}, failure(QStringLiteral("permission.denied"))};
    const int limit = parameters.value(QStringLiteral("limit")).toInt(100);
    if (limit < 1 || limit > 200) return {{}, failure(QStringLiteral("request.invalid"))};
    int offset = 0;
    const QString cursor = parameters.value(QStringLiteral("cursor")).toString();
    if (!cursor.isEmpty()) {
        const auto it = cursors_.constFind(cursor);
        if (it == cursors_.cend() || it->queryRef != queryRef) return {{}, failure(QStringLiteral("query.expired"))};
        offset = it->offset;
    }
    QJsonArray items;
    while (offset < found->charts.size() && items.size() < limit) items.append(found->charts[offset++]);
    QJsonValue next = QJsonValue::Null;
    if (offset < found->charts.size()) {
        if (cursors_.size() >= 10000) return {{}, failure(QStringLiteral("query.expired"))};
        const QString nextCursor = reference("cursor_");
        cursors_.insert(nextCursor, {queryRef, offset});
        next = nextCursor;
    }
    return {QJsonObject{{QStringLiteral("queryRef"), queryRef}, {QStringLiteral("items"), items},
        {QStringLiteral("nextCursor"), next}, {QStringLiteral("matchedCount"), found->charts.size()},
        {QStringLiteral("completeness"), QStringLiteral("unknown")}}, {}};
}

NetCallResult NetProvider::call(const QString& principal, const QString& operation, const QJsonObject& parameters)
{
    if (operation.startsWith("net.accounts.")) return accountCall(principal, operation, parameters);
    if (operation == QLatin1String("net.previews.release")) {
        const QString previewRef = parameters.value("previewRef").toString();
        if (previews_.value(previewRef).principal != principal) return {{}, failure("permission.denied")};
        previews_.remove(previewRef);
        return {QJsonObject{{"released", true}}, {}};
    }
    if (operation == QLatin1String("net.queries.results")) return queryPage(principal, parameters);
    if (operation == QLatin1String("net.providers.list")) {
        return {QJsonArray{QJsonObject{{QStringLiteral("providerId"), QStringLiteral("majdata")},
            {QStringLiteral("protocolVersion"), QStringLiteral("api3")}, {QStringLiteral("available"), true},
            {QStringLiteral("supportedOperations"), QJsonArray{QStringLiteral("net.queries.create"),
                QStringLiteral("net.queries.results"), QStringLiteral("net.probes.create"),
                QStringLiteral("net.downloads.create"), QStringLiteral("net.previews.prepare"), QStringLiteral("net.previews.release"),
                QStringLiteral("net.accounts.login"), QStringLiteral("net.accounts.list"), QStringLiteral("net.accounts.logout"),
                QStringLiteral("net.uploads.scan"), QStringLiteral("net.uploads.create")}},
            {QStringLiteral("resources"), QJsonArray{QStringLiteral("chart"), QStringLiteral("track"),
                QStringLiteral("image"), QStringLiteral("video")}},
            {QStringLiteral("requiresAccount"), false},
            {QStringLiteral("limits"), QJsonObject{{QStringLiteral("queryResponseBytes"), QStringLiteral("16777216")}}}}}, {}};
    }
    return {{}, failure(QStringLiteral("capability.unavailable"))};
}

} // namespace miacode::net
