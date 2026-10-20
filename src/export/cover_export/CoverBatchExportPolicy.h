#pragma once

#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QtGlobal>

namespace miacode::cover_export {

inline double boundedBatchCoverFrameSeconds(double requested, double chartDuration,
                                            double pvSeekEndSeconds = -1.0)
{
    const double end = pvSeekEndSeconds >= 0.0
        ? qMin(chartDuration, pvSeekEndSeconds) : chartDuration;
    return qBound(0.0, requested, qMax(0.0, end));
}

inline QString batchCoverOutputPath(const QString& outputDirectory, const QString& exportPath)
{
    return QDir(outputDirectory).filePath(QFileInfo(exportPath).completeBaseName()
                                        + QStringLiteral("_cover.png"));
}

} // namespace miacode::cover_export
