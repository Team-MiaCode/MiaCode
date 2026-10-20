#pragma once

#include <QtGlobal>

#include "export/video_export/VideoExportController.h"

struct VideoExportSnapshot;

namespace miacode::video_export {

struct ExportEstimateProfile {
    int outputWidth = 0;
    int outputHeight = 0;
    int fps = 0;
    double durationSeconds = 0.0;
    VideoExportPreset preset = VideoExportPreset::HighQuality;
    VideoExportSizePreset sizePreset = VideoExportSizePreset::Standard;
    VideoExportOutputMode outputMode = VideoExportOutputMode::Mp4;
};

ExportEstimateProfile exportEstimateProfileForSnapshot(const ::VideoExportSnapshot& snapshot);

// Returns a local, anonymous performance prediction. No chart names, paths,
// media names, authors, or note data are read or persisted by this component.
qint64 predictedExportTotalMs(
    const ExportEstimateProfile& profile,
    const QString& historyPathOverride = QString());

bool recordSuccessfulExportPerformance(
    const ExportEstimateProfile& profile,
    qint64 elapsedMs,
    const QString& historyPathOverride = QString());

QString exportEstimateHistoryPath();

}  // namespace miacode::video_export
