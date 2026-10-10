#include "media_tools/net/NetQueryRules.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <limits>

namespace miacode::net {

QString normalizedNetTag(const QString& tag)
{
    QString value = tag.trimmed();
    if (value.startsWith(QStringLiteral("tag:"), Qt::CaseInsensitive)) {
        value = value.mid(4).trimmed();
    }
    return value;
}

QString validateNetQuery(const NetQueryRequest& request)
{
    if (request.providerId != QLatin1String("majdata")) {
        return QStringLiteral("capability.unavailable");
    }
    if (request.uploader.trimmed().isEmpty() && normalizedNetTag(request.tag).isEmpty()
        && request.title.trimmed().isEmpty()) {
        return QStringLiteral("request.invalid");
    }
    if (request.startDate.isValid() != request.endDate.isValid() || !request.timeZone.isValid()) {
        return QStringLiteral("request.invalid");
    }
    const QStringList sorts{QStringLiteral("level_asc"), QStringLiteral("level_desc"),
        QStringLiteral("uploaded_asc"), QStringLiteral("uploaded_desc"),
        QStringLiteral("title_asc"), QStringLiteral("title_desc")};
    if (!sorts.contains(request.sort)) {
        return QStringLiteral("request.invalid");
    }
    if (request.endDate.isValid()
        && !qMax(request.startDate, request.endDate).addDays(1).isValid()) {
        return QStringLiteral("request.invalid");
    }
    return {};
}

QStringList netCandidateSearches(const NetQueryRequest& request)
{
    const QString uploader = request.uploader.trimmed();
    const QString tag = normalizedNetTag(request.tag);
    const QString title = request.title.trimmed();
    QStringList searches;
    if (!uploader.isEmpty()) {
        searches << QStringLiteral("uploader:%1").arg(uploader);
        if (!request.caseSensitive) {
            searches << QStringLiteral("uploader:%1").arg(uploader.toLower())
                     << QStringLiteral("uploader:%1").arg(uploader.toUpper())
                     << uploader << uploader.toLower() << uploader.toUpper();
        }
    } else if (!tag.isEmpty()) {
        searches << QStringLiteral("tag:%1").arg(tag) << tag;
        if (!request.caseSensitive) {
            searches << QStringLiteral("tag:%1").arg(tag.toLower())
                     << tag.toLower() << QStringLiteral("tag:%1").arg(tag.toUpper())
                     << tag.toUpper();
        }
    } else if (!title.isEmpty()) {
        searches << title;
        if (!request.caseSensitive) {
            searches << title.toLower() << title.toUpper();
        }
    }
    searches.removeDuplicates();
    return searches;
}

QString netCandidateCacheKey(const NetQueryRequest& request)
{
    QString field;
    QString value;
    if (!request.uploader.trimmed().isEmpty()) {
        field = QStringLiteral("uploader");
        value = request.uploader.trimmed();
    } else if (!normalizedNetTag(request.tag).isEmpty()) {
        field = QStringLiteral("tag");
        value = normalizedNetTag(request.tag);
    } else {
        field = QStringLiteral("title");
        value = request.title.trimmed();
    }
    if (!request.caseSensitive) value = value.toCaseFolded();
    return QString::fromUtf8(QJsonDocument(QJsonArray{
        request.providerId, field, value, request.caseSensitive}).toJson(QJsonDocument::Compact));
}

double highestNetLevel(const NetChartSummary& chart)
{
    double highest = -1.0;
    static const QRegularExpression pattern(QStringLiteral("^([0-9]+(?:\\.[0-9]+)?)(\\+)?$"));
    for (const QString& level : chart.levels) {
        const auto match = pattern.match(level.trimmed());
        if (!match.hasMatch()) continue;
        bool valid = false;
        const double value = QLocale::c().toDouble(match.captured(1), &valid);
        if (valid) highest = qMax(highest, value + (match.captured(2).isEmpty() ? 0.0 : 0.5));
    }
    return highest;
}

QList<NetChartSummary> filterAndSortNetCharts(
    const QList<NetChartSummary>& candidates, const NetQueryRequest& request)
{
    if (!validateNetQuery(request).isEmpty()) return {};
    const Qt::CaseSensitivity sensitivity = request.caseSensitive
        ? Qt::CaseSensitive : Qt::CaseInsensitive;
    const QString uploader = request.uploader.trimmed();
    const QString tag = normalizedNetTag(request.tag);
    const QString title = request.title.trimmed();
    const QDate start = qMin(request.startDate, request.endDate);
    const QDate end = qMax(request.startDate, request.endDate);
    QSet<QString> seen;
    QList<NetChartSummary> result;
    for (const auto& chart : candidates) {
        if (chart.id.isEmpty() || !chart.timestampUtc.isValid() || seen.contains(chart.id)) continue;
        if (!uploader.isEmpty() && chart.uploader.compare(uploader, sensitivity) != 0) continue;
        if (!title.isEmpty() && !chart.title.contains(title, sensitivity)) continue;
        if (!tag.isEmpty() && std::none_of(chart.publicTags.cbegin(), chart.publicTags.cend(),
                [&](const QString& value) { return value.contains(tag, sensitivity); })) continue;
        if (start.isValid()) {
            // Comparing local dates includes both boundary days even across DST transitions.
            const QDate date = chart.timestampUtc.toTimeZone(request.timeZone).date();
            if (date < start || date > end) continue;
        }
        seen.insert(chart.id);
        result.append(chart);
    }
    sortNetCharts(result, request.sort);
    return result;
}

void sortNetCharts(QList<NetChartSummary>& charts, const QString& sort)
{
    const bool descending = sort.endsWith(QStringLiteral("_desc"));
    std::sort(charts.begin(), charts.end(), [&](const auto& left, const auto& right) {
        int order = 0;
        if (sort.startsWith(QStringLiteral("level_"))) {
            const double a = highestNetLevel(left), b = highestNetLevel(right);
            if ((a < 0.0) != (b < 0.0)) return a >= 0.0;
            order = a < b ? -1 : a > b ? 1 : 0;
        } else if (sort.startsWith(QStringLiteral("uploaded_"))) {
            order = left.timestampUtc < right.timestampUtc ? -1
                : left.timestampUtc > right.timestampUtc ? 1 : 0;
        } else {
            order = QString::compare(left.title.toCaseFolded(), right.title.toCaseFolded());
        }
        if (order != 0) return descending ? order > 0 : order < 0;
        order = QString::compare(left.title.toCaseFolded(), right.title.toCaseFolded());
        return order != 0 ? order < 0 : left.id < right.id;
    });
}

NetCandidateCache::NetCandidateCache(Clock clock) : clock_(std::move(clock)) {}

std::optional<NetCandidateCache::Candidates> NetCandidateCache::find(const NetQueryRequest& request)
{
    if (request.forceRefresh) return std::nullopt;
    const auto it = entries_.find(netCandidateCacheKey(request));
    if (it == entries_.end()) return std::nullopt;
    if (clock_() >= it->expiresAt) {
        entries_.erase(it);
        return std::nullopt;
    }
    return it->candidates;
}

void NetCandidateCache::insert(const NetQueryRequest& request, const QList<NetChartSummary>& candidates, int skippedRows)
{
    const qint64 now = clock_();
    for (auto it = entries_.begin(); it != entries_.end();) {
        if (now >= it->expiresAt) it = entries_.erase(it);
        else ++it;
    }
    if (entries_.size() >= 100) entries_.clear();
    entries_.insert(netCandidateCacheKey(request), {{candidates, skippedRows}, now + 300000});
}

void NetCandidateCache::clear() { entries_.clear(); }

} // namespace miacode::net
