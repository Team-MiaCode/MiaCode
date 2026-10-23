#include "core/analysis/MuriSlideReferenceData.h"

#include "core/chart/SlideReferenceData.h"

#include <QHash>
#include <QMutex>
#include <QMutexLocker>

namespace miacode::muri::detail {

QVector<QStringList> loadPadAreaSequence(const QJsonArray& areaArray)
{
    QVector<QStringList> sequence;
    sequence.reserve(areaArray.size());
    for (const QJsonValue& areaValue : areaArray) {
        QStringList pads;
        for (const QJsonValue& padValue : areaValue.toArray()) {
            const QString pad = padValue.toString();
            if (!pad.isEmpty()) {
                pads.append(pad);
            }
        }
        if (!pads.isEmpty()) {
            sequence.append(pads);
        }
    }
    return sequence;
}

QVector<QVector<QStringList>> loadTriPadAreaSequence(const QJsonArray& laneArray)
{
    QVector<QVector<QStringList>> sequence;
    sequence.reserve(laneArray.size());
    for (const QJsonValue& value : laneArray) {
        sequence.append(loadPadAreaSequence(value.toArray()));
    }
    return sequence;
}

QVector<QPointF> loadRuntimeSlideActionPath(const QString& key)
{
    static QMutex mutex;
    static QHash<QString, QVector<QPointF>> cache;
    {
        QMutexLocker lock(&mutex);
        const auto it = cache.constFind(key);
        if (it != cache.constEnd()) return it.value();
    }

    QVector<QPointF> points;
    const QJsonObject entry = slide_reference::root().value(QStringLiteral("slides")).toObject().value(key).toObject();
    if (entry.isEmpty()) {
        return points;
    }

    const QJsonArray sampleArray = entry.value(QStringLiteral("real_path_samples")).toArray();
    points.reserve(sampleArray.size());
    for (const QJsonValue& sampleValue : sampleArray) {
        const QJsonObject sampleObject = sampleValue.toObject();
        points.append(QPointF(
            sampleObject.value(QStringLiteral("x")).toDouble(),
            sampleObject.value(QStringLiteral("y")).toDouble()));
    }
    QMutexLocker lock(&mutex);
    cache.insert(key, points);
    return points;
}

QVector<QVector<QPointF>> loadRuntimeWifiActionPaths(const QString& key)
{
    static QMutex mutex;
    static QHash<QString, QVector<QVector<QPointF>>> cache;
    {
        QMutexLocker lock(&mutex);
        const auto it = cache.constFind(key);
        if (it != cache.constEnd()) return it.value();
    }

    QVector<QVector<QPointF>> paths;
    const QJsonObject entry = slide_reference::root().value(QStringLiteral("wifi")).toObject().value(key).toObject();
    if (entry.isEmpty()) {
        return paths;
    }

    const QJsonArray pathArray = entry.value(QStringLiteral("di_real_path_samples")).toArray();
    paths.reserve(pathArray.size());
    for (const QJsonValue& pathValue : pathArray) {
        QVector<QPointF> points;
        const QJsonArray sampleArray = pathValue.toArray();
        points.reserve(sampleArray.size());
        for (const QJsonValue& sampleValue : sampleArray) {
            const QJsonObject sampleObject = sampleValue.toObject();
            points.append(QPointF(
                sampleObject.value(QStringLiteral("x")).toDouble(),
                sampleObject.value(QStringLiteral("y")).toDouble()));
        }
        if (!points.isEmpty()) {
            paths.append(points);
        }
    }
    QMutexLocker lock(&mutex);
    cache.insert(key, paths);
    return paths;
}

}  // namespace miacode::muri::detail
