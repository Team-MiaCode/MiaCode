#include "media_tools/net/NetQueryRules.h"

#include <QCoreApplication>
#include <QTextStream>

namespace {
bool check(bool condition, const char* message)
{
    if (!condition) QTextStream(stderr) << "FAIL: " << message << '\n';
    return condition;
}

miacode::net::NetChartSummary chart(
    const char* id, const char* title, const char* uploader, const char* tag,
    const char* time, const char* level)
{
    miacode::net::NetChartSummary result;
    result.id = QString::fromUtf8(id);
    result.title = QString::fromUtf8(title);
    result.uploader = QString::fromUtf8(uploader);
    result.publicTags = {QString::fromUtf8(tag)};
    result.timestampUtc = QDateTime::fromString(QString::fromUtf8(time), Qt::ISODate);
    result.levels = {QString::fromUtf8(level)};
    return result;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    using namespace miacode::net;
    bool ok = true;
    NetQueryRequest request;
    request.uploader = QStringLiteral(" Alice ");
    request.tag = QStringLiteral(" TAG: event ");
    request.title = QStringLiteral("song");
    request.timeZone = QTimeZone("Asia/Shanghai");
    request.startDate = request.endDate = QDate(2026, 10, 9);
    QList<NetChartSummary> candidates{
        chart("a", "Song A", "alice", "Event", "2026-10-08T16:00:00Z", "13+"),
        chart("b", "Song B", "ALICE", "event", "2026-10-09T15:59:59Z", "13"),
        chart("c", "Song C", "bob", "event", "2026-10-09T00:00:00Z", "14"),
        chart("d", "Other", "alice", "event", "2026-10-09T00:00:00Z", "14"),
        chart("e", "Song E", "alice", "else", "2026-10-09T00:00:00Z", "14"),
        chart("f", "Song F", "alice", "event", "2026-10-09T16:00:00Z", "14")};
    candidates.append(candidates.first());
    const auto filtered = filterAndSortNetCharts(candidates, request);
    ok &= check(filtered.size() == 2 && filtered.at(0).id == QStringLiteral("b")
        && filtered.at(1).id == QStringLiteral("a"), "all filters are AND, local boundary days and duplicate IDs are respected");
    request.caseSensitive = true;
    ok &= check(filterAndSortNetCharts(candidates, request).isEmpty(), "case-sensitive uploader requires exact equality");
    request.caseSensitive = false;
    request.sort = QStringLiteral("level_desc");
    const auto levels = filterAndSortNetCharts(candidates, request);
    ok &= check(levels.size() == 2 && levels.first().id == QStringLiteral("a"), "plus levels add 0.5");
    candidates[0].levels = {QStringLiteral("invalid")};
    request.sort = QStringLiteral("level_asc");
    const auto invalidLevels = filterAndSortNetCharts(candidates, request);
    ok &= check(invalidLevels.first().id == QStringLiteral("b"), "invalid levels sort last in both directions");

    NetQueryRequest dst;
    dst.title = QStringLiteral("song");
    dst.timeZone = QTimeZone("America/New_York");
    dst.startDate = dst.endDate = QDate(2026, 11, 1);
    const QList<NetChartSummary> dstCharts{
        chart("1", "song", "u", "t", "2026-11-01T03:59:59Z", "1"),
        chart("2", "song", "u", "t", "2026-11-01T04:00:00Z", "1"),
        chart("3", "song", "u", "t", "2026-11-02T04:59:59Z", "1"),
        chart("4", "song", "u", "t", "2026-11-02T05:00:00Z", "1")};
    const auto dstResult = filterAndSortNetCharts(dstCharts, dst);
    ok &= check(dstResult.size() == 2 && dstResult.first().id == QStringLiteral("3"), "25-hour DST boundary includes exactly one local day");
    dst.startDate = QDate(2026, 11, 2);
    dst.endDate = QDate(2026, 11, 1);
    ok &= check(filterAndSortNetCharts(dstCharts, dst).size() == 3, "reversed date ranges normalize");
    dst.endDate = {};
    ok &= check(validateNetQuery(dst) == QStringLiteral("request.invalid"), "single-ended date range fails validation");
    NetQueryRequest empty;
    ok &= check(!validateNetQuery(empty).isEmpty(), "query requires a search field");
    request.sort = QStringLiteral("unknown");
    ok &= check(!validateNetQuery(request).isEmpty(), "unknown sorting is rejected");
    request.sort = QStringLiteral("uploaded_desc");
    ok &= check(netCandidateSearches(request).first() == QStringLiteral("uploader:Alice")
        && !netCandidateSearches(request).contains(QStringLiteral("song")), "uploader owns candidate source when filters combine");

    qint64 clock = 0;
    NetCandidateCache cache([&] { return clock; });
    cache.insert(request, candidates);
    request.title = QStringLiteral("other");
    ok &= check(cache.find(request).has_value()
        && filterAndSortNetCharts(cache.find(request)->charts, request).size() == 1,
        "cached candidates reapply non-source filters");
    request.forceRefresh = true;
    ok &= check(!cache.find(request).has_value(), "force refresh bypasses cache");
    request.forceRefresh = false;
    request.caseSensitive = true;
    ok &= check(!cache.find(request).has_value(), "case mode isolates cache");
    request.caseSensitive = false;
    clock = 299999;
    ok &= check(cache.find(request).has_value(), "candidate cache is valid before deadline");
    clock = 300000;
    ok &= check(!cache.find(request).has_value(), "candidate cache expires at five minutes");
    return ok ? 0 : 1;
}
