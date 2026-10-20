#include "export/video_export/VideoExportController.h"

#include "export/video_export/BassExportAudioBackend.h"
#include "export/video_export/RawVideoPipeTransport.h"
#include "export/video_export/VideoExportAudioRenderPlan.h"
#include "export/video_export/VideoExportQuickRenderBackend.h"
#include "export/video_export/VideoExportRuntimePolicy.h"
#include "core/video/AssetPaths.h"
#include "core/chart/ChartAssetPaths.h"
#include "core/chart/IntroConfig.h"
#include "common/DebugLog.h"
#include "common/OperationLog.h"
#include "common/DebugOptions.h"
#include "core/video/LayoutRingConfig.h"
#include "audio/PreviewAudioMixConfig.h"
#include "core/video/PreviewGameplayConfig.h"
#include "core/scene/PreviewSceneGeometry.h"
#include "core/scene/PreviewSfxTimeline.h"
#include "preview/runtime/PreviewSceneAssetLoader.h"
#include "core/analysis/MuriAnalyzer.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QImageReader>
#include <QMutex>
#include <QMutexLocker>
#include <QPainter>
#include <QProcess>
#include <QRect>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>
#include <QUuid>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>
#include <functional>
#include <limits>
#include <optional>

#ifdef Q_OS_WIN
#include <windows.h>
#endif
#include "export/video_export/VideoExportControllerInternal.h"

// Core translation unit for VideoExportController. After the god-file split this
// keeps only the thin VideoExportController::exportFullPreview entry point; every
// free helper now lives in namespace miacode::video_export::detail across the
// VideoExport{Encoder,Diagnostics,FrameRender,Pipeline}.cpp group TUs (declared in
// VideoExportControllerInternal.h), and exportPreparedTask lives in
// VideoExportPreparedTask.cpp.
using namespace miacode::video_export::detail;

VideoExportResult VideoExportController::exportFullPreview(
    const VideoExportTask& task
)
{
    MC_OP("VideoExportController::exportFullPreview");
    _mc_op_.note(QStringLiteral("output=%1").arg(task.outputPath));
    return exportPreparedTask(task);
}

QString VideoExportController::ffmpegExecutablePath()
{
    return resolveFfmpegExecutable();
}
