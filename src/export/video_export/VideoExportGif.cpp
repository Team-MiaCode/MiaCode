#include "export/video_export/VideoExportGif.h"

#include "core/chart/ChartAssetPaths.h"
#include "export/video_export/VideoExportControllerInternal.h"
#include "export/video_export/VideoExportQuickRenderBackend.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>
#include <cmath>
#include <limits>

namespace miacode::video_export {
using namespace detail;

void applyGifExportSettings(VideoExportTask& task)
{
    task.outputMode = VideoExportOutputMode::Gif;
    task.outputWidth = 320;
    task.outputHeight = 320;
    task.fps = 24;
    task.fullRangeExport = false;
    task.intro.enabled = false;
    task.clockCountEnabled = false;
    task.showTimestamp = false;
    task.showObjectStatsHud = false;
    task.showChartInfoHud = false;
    task.centerDisplayMode = miacode::preview_gameplay::CenterDisplayMode::Off;
    task.previewMaxOutputSeconds = 0.0;
}

VideoExportResult exportGifTask(const VideoExportTask& requestedTask,
                                const VideoExportProgressCallback& progressCallback)
{
    VideoExportTask task = requestedTask;
    applyGifExportSettings(task);
    const QSize frameSize(task.outputWidth, task.outputHeight);
    VideoExportResult result;
    const auto fail = [&result](const QString& message, const QString& details = QString()) {
        result.message = message;
        result.details = withExportLogPath(details);
        appendVideoExportLog(QStringLiteral("fail_gif_export"), message + QLatin1Char(' ') + details);
        return result;
    };
    const auto progress = [&progressCallback](int percent, const QString& text) {
        return progressCallback && progressCallback(percent, text);
    };
    const auto canceled = [&result]() {
        result.message = QStringLiteral("canceled");
        return result;
    };
    if (task.outputPath.isEmpty() || task.skinDirectory.isEmpty()
        || !std::isfinite(task.exportStartSeconds) || task.exportStartSeconds < 0.0
        || !std::isfinite(task.contentDurationSeconds) || task.contentDurationSeconds <= 0.0
        || task.contentDurationSeconds >= static_cast<double>(std::numeric_limits<qint64>::max()) / task.fps) {
        return fail(QStringLiteral("Export parameters are invalid."));
    }
    const qint64 frameCount = qMax<qint64>(1, qRound64(task.contentDurationSeconds * task.fps));
    const QString ffmpegPath = resolveFfmpegExecutable();
    if (ffmpegPath.isEmpty()) return fail(QStringLiteral("ffmpeg executable was not found."));

    const QString tempPattern = exportTemporaryDirectoryPattern(QStringLiteral("gif"));
    if (tempPattern.isEmpty()) return fail(QStringLiteral("Unable to create temporary directory."));
    QTemporaryDir tempDir(tempPattern);
    if (!tempDir.isValid()) return fail(QStringLiteral("Unable to create temporary directory."));
    ScopedExportTempDirTracker tracker(tempDir.path());
    const QDir directory(tempDir.path());
    const QString videoPath = directory.filePath(QStringLiteral("frames.mkv"));
    const QString palettePath = directory.filePath(QStringLiteral("palette.png"));
    const QString stagedPath = directory.filePath(QStringLiteral("output.gif"));

    QString mediaPath = task.backgroundMediaPath;
    if (!miacode::chart_assets::isSupportedBackgroundMediaPath(mediaPath)) {
        mediaPath = miacode::chart_assets::resolvePreferredBackgroundMediaPath(task.chartPath, mediaPath);
    }
    VideoExportQuickRenderBackend renderer;
    QString error;
    // The complete marker set preserves notes and effects crossing either boundary.
    if (!renderer.bootstrap(task, mediaPath, task.noteMarkers, task.muriAnalysisReport,
                            frameSize, &error)) {
        return fail(QStringLiteral("Failed to load export skin assets."), error);
    }
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(3, 2);
    format.setProfile(QSurfaceFormat::CoreProfile);
    if (!renderer.initializeExportRenderer(format, nullptr, &error)) {
        return fail(QStringLiteral("Failed to initialize Quick export renderer."), error);
    }
    appendVideoExportLog(QStringLiteral("render_backend"),
        QStringLiteral("gif=1 size=%1x%2 fps=%3 start=%4 frames=%5 renderer=%6")
            .arg(frameSize.width()).arg(frameSize.height()).arg(task.fps)
            .arg(task.exportStartSeconds, 0, 'f', 6).arg(frameCount)
            .arg(renderer.adapterOrRendererForDebug()));
    if (progress(2, QStringLiteral("Preparing export..."))) return canceled();
    // Prime textures and shaders without adding a frame to the exported timeline.
    const QImage warmupFrame = renderer.renderFrameOffscreen(
        frameSize, task.exportStartSeconds, false, false);
    if (warmupFrame.isNull()) return fail(QStringLiteral("Render frame failed."));

    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    const auto runStage = [&](const QStringList& stageArgs, int percent, const QString& label) {
        if (progress(percent, label)) {
            result = canceled();
            return false;
        }
        QStringList args{QStringLiteral("-y"), QStringLiteral("-hide_banner"),
                         QStringLiteral("-loglevel"), QStringLiteral("error"),
                         QStringLiteral("-filter_complex_threads"), QStringLiteral("2"),
                         QStringLiteral("-filter_threads"), QStringLiteral("2")};
        args.append(stageArgs);
        appendVideoExportLog(QStringLiteral("gif_encode_args"), ffmpegBaseArgsLog(ffmpegPath, args));
        process.start(ffmpegPath, args, QIODevice::ReadWrite);
        if (!process.waitForStarted(5000)) {
            result = fail(QStringLiteral("Failed to start ffmpeg."), process.errorString());
            return false;
        }
        return true;
    };
    const auto finishStage = [&](int percent, const QString& label) {
        if (!waitForProcessWithProgress(process, QStringLiteral("gif_stage_wait_begin"),
                QStringLiteral("gif_stage_wait_done"), label, percent, progress,
                QStringLiteral("stage=gif_encode"), &result)) return false;
        if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
            result = fail(QStringLiteral("ffmpeg encode failed."), processOutputAndErrorForLog(process));
            return false;
        }
        process.readAll();
        return true;
    };

    if (!runStage({QStringLiteral("-f"), QStringLiteral("rawvideo"),
            QStringLiteral("-pixel_format"), QStringLiteral("rgba"),
            QStringLiteral("-video_size"), QStringLiteral("%1x%2").arg(frameSize.width()).arg(frameSize.height()),
            QStringLiteral("-framerate"), QString::number(task.fps),
            QStringLiteral("-i"), QStringLiteral("pipe:0"), QStringLiteral("-an"),
            QStringLiteral("-c:v"), QStringLiteral("ffv1"),
            QStringLiteral("-level"), QStringLiteral("3"),
            QStringLiteral("-pix_fmt"), QStringLiteral("bgra"),
            QStringLiteral("-threads"), QStringLiteral("4"), videoPath},
            3, QStringLiteral("Starting ffmpeg..."))) return result;

    for (qint64 frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
        const int percent = 5 + static_cast<int>(static_cast<double>(frameIndex) / frameCount * 65.0);
        if (progress(percent, QStringLiteral("Rendering frames and encoding... %1/%2")
                .arg(frameIndex + 1).arg(frameCount))) {
            process.kill();
            process.waitForFinished(2000);
            return canceled();
        }
        ExportFrameRenderParams params;
        params.exportSecond = task.exportStartSeconds + static_cast<double>(frameIndex) / task.fps;
        QImage frame = renderExportFrameSynchronously(&renderer, true, frameSize, params)
            .convertToFormat(QImage::Format_RGBA8888);
        if (frame.isNull() || frame.bytesPerLine() != frameSize.width() * 4
            || !writeAllToProcess(&process, reinterpret_cast<const char*>(frame.constBits()),
                                  frame.sizeInBytes(), &error)) {
            process.kill();
            process.waitForFinished(2000);
            return fail(QStringLiteral("Render frame failed."), error + processOutputAndErrorForLog(process));
        }
    }
    process.closeWriteChannel();
    if (!finishStage(70, QStringLiteral("Finalizing encoded video stream..."))) return result;
    renderer.shutdownOffscreenRenderer();

    if (!runStage({QStringLiteral("-i"), videoPath, QStringLiteral("-vf"),
            QStringLiteral("palettegen=stats_mode=diff:reserve_transparent=1"),
            QStringLiteral("-frames:v"), QStringLiteral("1"),
            QStringLiteral("-threads"), QStringLiteral("2"), palettePath},
            72, QStringLiteral("Preparing GIF palette..."))
        || !finishStage(72, QStringLiteral("Preparing GIF palette..."))) return result;

    if (!runStage({QStringLiteral("-i"), videoPath, QStringLiteral("-i"), palettePath,
            QStringLiteral("-lavfi"),
            QStringLiteral("paletteuse=dither=bayer:bayer_scale=3:diff_mode=rectangle"),
            QStringLiteral("-fps_mode"), QStringLiteral("passthrough"),
            QStringLiteral("-loop"), QStringLiteral("0"),
            QStringLiteral("-final_delay"), QString::number(qRound(100.0 / task.fps)),
            QStringLiteral("-threads"), QStringLiteral("2"), stagedPath},
            82, QStringLiteral("Encoding GIF..."))
        || !finishStage(82, QStringLiteral("Encoding GIF..."))) return result;

    if (progress(98, QStringLiteral("Collecting export summary..."))) return canceled();
    if (!replaceOutputFileAtomicallyBestEffort(stagedPath, task.outputPath, &error)) {
        return fail(QStringLiteral("Failed to finalize GIF output file."), error);
    }
    result.success = true;
    result.message = QStringLiteral("ok");
    appendVideoExportLog(QStringLiteral("export_success"),
        QStringLiteral("gif=%1 frames=%2 sizeBytes=%3")
            .arg(task.outputPath).arg(frameCount).arg(QFileInfo(task.outputPath).size()));
    return result;
}

} // namespace miacode::video_export
