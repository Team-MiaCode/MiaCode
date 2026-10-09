#pragma once
#include "core/chart/document/SimaiDocument.h"
#include "tools/video_export/VideoExportController.h"

namespace miacode::android {
// Reads one immutable chart, never the active Android workspace.
bool buildMobileChartExportTask(const SimaiDocument& document, int difficultyId,
    const QString& chartPath, double trackDuration, const VideoExportTask& settings,
    VideoExportTask* task, QString* error = nullptr, bool validateSyntax = true);
QString mobileExportFileStem(const QString& title, const QString& fallback);
}
