#pragma once
#include "app/services/ExportEngine.h"
#include <atomic>

namespace miacode::android {
struct MobileBatchExportJob { VideoExportTask task; QString displayName; };
struct MobileBatchExportPlan {
    QVector<MobileBatchExportJob> jobs;
    QStringList failures;
    bool canceled = false;
};
// Runs on the preparation worker. Documents and settings are captured before
// any export starts; cancellation also covers media-duration probing.
MobileBatchExportPlan prepareMobileBatchExport(const QStringList& directories, const QList<int>& difficulties,
    const QString& outputDirectory, const VideoExportTask& settings, const std::atomic_bool& canceled);
}
