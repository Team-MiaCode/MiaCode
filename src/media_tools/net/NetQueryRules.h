#pragma once

#include "media_tools/net/NetChart.h"

#include <QDate>
#include <QHash>
#include <QTimeZone>

#include <functional>
#include <optional>

namespace miacode::net {

struct NetQueryRequest {
    QString providerId = QStringLiteral("majdata");
    QString uploader;
    QString tag;
    QString title;
    QDate startDate;
    QDate endDate;
    QTimeZone timeZone = QTimeZone::systemTimeZone();
    bool caseSensitive = false;
    bool forceRefresh = false;
    QString sort = QStringLiteral("uploaded_desc");
};

QString normalizedNetTag(const QString& tag);
QString validateNetQuery(const NetQueryRequest& request);
QStringList netCandidateSearches(const NetQueryRequest& request);
QString netCandidateCacheKey(const NetQueryRequest& request);
QList<NetChartSummary> filterAndSortNetCharts(
    const QList<NetChartSummary>& candidates, const NetQueryRequest& request);
double highestNetLevel(const NetChartSummary& chart);
void sortNetCharts(QList<NetChartSummary>& charts, const QString& sort);

// Only candidates are cached. Every query reapplies its complete filter and sort.
class NetCandidateCache {
public:
    struct Candidates {
        QList<NetChartSummary> charts;
        int skippedRows = 0;
    };
    using Clock = std::function<qint64()>;
    explicit NetCandidateCache(Clock clock);
    std::optional<Candidates> find(const NetQueryRequest& request);
    void insert(const NetQueryRequest& request, const QList<NetChartSummary>& candidates, int skippedRows = 0);
    void clear();

private:
    struct Entry {
        Candidates candidates;
        qint64 expiresAt = 0;
    };
    Clock clock_;
    QHash<QString, Entry> entries_;
};

} // namespace miacode::net
