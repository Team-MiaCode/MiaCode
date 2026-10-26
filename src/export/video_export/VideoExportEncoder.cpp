#include "export/video_export/VideoExportController.h"

#include "export/video_export/BassExportAudioBackend.h"
#include "export/video_export/RawVideoPipeTransport.h"
#include "export/video_export/VideoExportAudioRenderPlan.h"
#include "export/video_export/VideoExportQuickRenderBackend.h"
#include "export/video_export/VideoExportRuntimePolicy.h"
#include "export/video_export/EncoderProbeCache.h"
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
#include <iterator>
#include <limits>
#include <optional>

#ifdef Q_OS_WIN
#include <windows.h>
#endif
#include "export/video_export/VideoExportControllerInternal.h"

// VideoExportEncoder.cpp — memory probe, x264/bitrate/runtime-config selection, ffmpeg/ffprobe resolution, encoder runtime probe, and encoder selection.
//
// Definitions extracted verbatim from the original VideoExportController.cpp
// during the god-file split. All helpers live in the shared
// miacode::video_export::detail namespace (declared in
// VideoExportControllerInternal.h).
using namespace miacode::video_export::detail;

namespace miacode::video_export::detail {

namespace {

QString preferredHardwareEncoder()
{
    const auto cache = miacode::video_export::encoderProbeCache();
    return cache ? cache->preferredHardwareEncoder().trimmed() : QString();
}

void rememberPreferredHardwareEncoder(const QString& codec)
{
    const auto cache = miacode::video_export::encoderProbeCache();
    if (cache && !codec.isEmpty()) {
        cache->rememberPreferredHardwareEncoder(codec);
    }
}

}  // namespace

qint64 bytesToMiB(quint64 bytes)
{
    return static_cast<qint64>(bytes / (1024ULL * 1024ULL));
}

SystemMemoryInfo querySystemMemoryInfo()
{
    SystemMemoryInfo info;
#ifdef Q_OS_WIN
    MEMORYSTATUSEX statex{};
    statex.dwLength = sizeof(statex);
    if (GlobalMemoryStatusEx(&statex) != 0) {
        info.valid = true;
        info.totalPhysicalBytes = statex.ullTotalPhys;
        info.availablePhysicalBytes = statex.ullAvailPhys;
    }
#endif
    return info;
}

QString memoryInfoToLog(const SystemMemoryInfo& info)
{
    if (!info.valid) {
        return QStringLiteral("memory_snapshot unavailable");
    }
    return QStringLiteral("memory_snapshot totalMiB=%1 availMiB=%2")
        .arg(bytesToMiB(info.totalPhysicalBytes))
        .arg(bytesToMiB(info.availablePhysicalBytes));
}

ExportRuntimeConfig loadExportRuntimeConfig()
{
    ExportRuntimeConfig config;
    config.forcedEncoder = qEnvironmentVariable("MIACODE_EXPORT_FORCE_ENCODER").trimmed();
    config.skipEncoderRuntimeProbe =
        miacode::debug_options::envFlagEnabled("MIACODE_EXPORT_SKIP_ENCODER_RUNTIME_PROBE");
    config.encoderThreadsOverride =
        miacode::debug_options::envIntValue("MIACODE_EXPORT_ENCODER_THREADS", -1);
    config.filterThreadsOverride =
        miacode::debug_options::envIntValue("MIACODE_EXPORT_FILTER_THREADS", -1);
    config.x264PresetOverride = qEnvironmentVariable("MIACODE_EXPORT_X264_PRESET").trimmed();
    config.x264CrfOverride = miacode::debug_options::envIntValue("MIACODE_EXPORT_X264_CRF", -1);
    config.x264BframesOverride = miacode::debug_options::envIntValue("MIACODE_EXPORT_X264_BFRAMES", -1);

    const std::optional<bool> gpuRenderOverride =
        miacode::debug_options::envOptionalFlagValue("MIACODE_EXPORT_ENABLE_GPU_RENDER");
    const std::optional<bool> enablePboOverride =
        miacode::debug_options::envOptionalFlagValue("MIACODE_EXPORT_ENABLE_OFFSCREEN_PBO");
    const std::optional<bool> disablePboOverride =
        miacode::debug_options::envOptionalFlagValue("MIACODE_EXPORT_DISABLE_OFFSCREEN_PBO");
    config.renderBackend.requestGpuRender =
        enablePboOverride.value_or(false) ? true : gpuRenderOverride.value_or(true);
    config.renderBackend.requestOffscreenPboReadback =
        miacode::video_export::shouldRequestOffscreenPboReadback(enablePboOverride, disablePboOverride);

    config.diag.repeatEnabled = miacode::debug_options::envFlagEnabled("MIACODE_EXPORT_DIAG_REPEAT");
    config.diag.cropBottom = qMax(0, miacode::debug_options::envIntValue("MIACODE_EXPORT_DIAG_CROP_BOTTOM", 0));
    config.diag.maxLines = qMax(0, miacode::debug_options::envIntValue("MIACODE_EXPORT_DIAG_MAX_LINES", 400));
    config.diag.logAllRepeatPairs =
        miacode::debug_options::envFlagEnabled("MIACODE_EXPORT_DIAG_LOG_ALL_REPEATS");
    const std::optional<bool> objectHashOverride =
        miacode::debug_options::envOptionalFlagValue("MIACODE_EXPORT_DIAG_OBJECT_HASH");
    config.diag.objectHashEnabled = objectHashOverride.value_or(true);
    config.diag.objectTraceEnabled =
        miacode::debug_options::envFlagEnabled("MIACODE_EXPORT_DIAG_OBJECT_TRACE");
    config.diag.objectTraceMaxLines = qMax(
        0,
        miacode::debug_options::envIntValue(
            "MIACODE_EXPORT_DIAG_OBJECT_TRACE_MAX_LINES",
            qMax(config.diag.maxLines, 5000)
        )
    );
    config.diag.objectDiffThreshold = qBound(
        0,
        miacode::debug_options::envIntValue("MIACODE_EXPORT_DIAG_OBJECT_DIFF_THRESHOLD", 8),
        4 * 255
    );
    config.diag.compareRenderPathsEnabled =
        miacode::debug_options::envFlagEnabled("MIACODE_EXPORT_DIAG_COMPARE_RENDER_PATHS");
    config.diag.compareRadius = qBound(
        2,
        miacode::debug_options::envIntValue("MIACODE_EXPORT_DIAG_COMPARE_RADIUS", 24),
        512
    );
    config.diag.compareMaxLines = qMax(
        0,
        miacode::debug_options::envIntValue("MIACODE_EXPORT_DIAG_COMPARE_MAX_LINES", config.diag.maxLines)
    );
    config.diag.compareLogThreshold = qBound(
        0.0,
        miacode::debug_options::envDoubleValue("MIACODE_EXPORT_DIAG_COMPARE_LOG_THRESHOLD", 0.0010),
        1.0
    );
    config.diag.pipeHashEnabled =
        miacode::debug_options::envFlagEnabled("MIACODE_EXPORT_DIAG_PIPE_HASH");
    config.diag.pipeHashMaxLines = qMax(
        0,
        miacode::debug_options::envIntValue("MIACODE_EXPORT_DIAG_PIPE_HASH_MAX_LINES", config.diag.maxLines)
    );
    config.diag.rawDumpPath = qEnvironmentVariable("MIACODE_EXPORT_DIAG_RAW_DUMP_PATH").trimmed();
    return config;
}

bool fileIsExecutable(const QString& path)
{
    const QFileInfo info(path);
    return info.exists() && info.isFile() && info.isExecutable();
}

QString resolveFfmpegExecutable()
{
#ifdef Q_OS_WIN
    const QString ffmpegName = QStringLiteral("ffmpeg.exe");
#else
    const QString ffmpegName = QStringLiteral("ffmpeg");
#endif
    const QString envPath = qEnvironmentVariable("MIACODE_FFMPEG_PATH", qEnvironmentVariable("MIACODE_FFMPEG"));
    if (fileIsExecutable(envPath)) {
        return normalizePath(envPath);
    }

    const QDir appDir(QCoreApplication::applicationDirPath());
    const QStringList appCandidates{
        appDir.filePath(ffmpegName),
        appDir.filePath(QStringLiteral("ffmpeg/%1").arg(ffmpegName)),
        appDir.filePath(QStringLiteral("../Resources/%1").arg(ffmpegName)),
        appDir.filePath(QStringLiteral("../../third_party/ffmpeg/windows/%1").arg(ffmpegName)),
        appDir.filePath(QStringLiteral("../../third_party/ffmpeg/macos/%1").arg(ffmpegName)),
        appDir.filePath(QStringLiteral("../../third_party/ffmpeg/linux/%1").arg(ffmpegName)),
        appDir.filePath(QStringLiteral("../../../third_party/ffmpeg/windows/%1").arg(ffmpegName)),
        appDir.filePath(QStringLiteral("../../../third_party/ffmpeg/macos/%1").arg(ffmpegName)),
        appDir.filePath(QStringLiteral("../../../third_party/ffmpeg/linux/%1").arg(ffmpegName)),
        QStringLiteral("/opt/homebrew/bin/%1").arg(ffmpegName),
        QStringLiteral("/usr/local/bin/%1").arg(ffmpegName),
    };
    for (const QString& candidate : appCandidates) {
        if (fileIsExecutable(candidate)) {
            return normalizePath(candidate);
        }
    }

    const QString fromPath = QStandardPaths::findExecutable(ffmpegName);
    if (fileIsExecutable(fromPath)) {
        return normalizePath(fromPath);
    }
    return QString();
}

QString resolveFfprobeExecutable(const QString& ffmpegPath)
{
#ifdef Q_OS_WIN
    const QString ffprobeName = QStringLiteral("ffprobe.exe");
#else
    const QString ffprobeName = QStringLiteral("ffprobe");
#endif
    if (!ffmpegPath.isEmpty()) {
        const QFileInfo ffmpegInfo(ffmpegPath);
        const QString sibling = ffmpegInfo.dir().filePath(ffprobeName);
        if (fileIsExecutable(sibling)) {
            return normalizePath(sibling);
        }
    }
    const QString fromPath = QStandardPaths::findExecutable(ffprobeName);
    if (fileIsExecutable(fromPath)) {
        return normalizePath(fromPath);
    }
    return QString();
}

bool hasEncoderToken(const QString& encodersOutput, const QString& encoderName)
{
    const QRegularExpression pattern(
        QStringLiteral("(?m)^\\s*\\S{6}\\s+%1(?:\\s|$)").arg(QRegularExpression::escape(encoderName))
    );
    return pattern.match(encodersOutput).hasMatch();
}

QString resolveVaapiDevicePath()
{
    const QString fromEnv = qEnvironmentVariable("MIACODE_EXPORT_VAAPI_DEVICE").trimmed();
    if (!fromEnv.isEmpty()) {
        return QFileInfo::exists(fromEnv) ? fromEnv : QString();
    }
    const QStringList preferred{
        QStringLiteral("/dev/dri/renderD128"),
        QStringLiteral("/dev/dri/renderD129"),
    };
    for (const QString& path : preferred) {
        if (QFileInfo::exists(path)) {
            return path;
        }
    }
    const QDir driDir(QStringLiteral("/dev/dri"));
    const QStringList renderNodes = driDir.entryList(
        QStringList{QStringLiteral("renderD*")},
        QDir::System,
        QDir::Name);
    if (!renderNodes.isEmpty()) {
        return driDir.absoluteFilePath(renderNodes.first());
    }
    return {};
}

bool probeHardwareEncoderRuntime(
    const QString& ffmpegPath,
    const VideoEncoderConfig& candidate,
    int outputWidth,
    int outputHeight,
    int fps,
    QString* detail
)
{
    QProcess probe;
    probe.setProcessChannelMode(QProcess::MergedChannels);
    const int safeFps = qMax(1, fps);
    const QSize probeSize(qMax(1, outputWidth), qMax(1, outputHeight));
    const int probeFrameCount = qBound(6, safeFps / 5, 12);
    const double probeDurationSeconds =
        qMax(0.10, static_cast<double>(probeFrameCount) / static_cast<double>(safeFps));
    QStringList args{
        QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"), QStringLiteral("error"),
    };
    if (candidate.needsVaapiHwUpload) {
        if (candidate.vaapiDevicePath.isEmpty()) {
            if (detail != nullptr) {
                *detail = QStringLiteral("vaapi_device_missing");
            }
            return false;
        }
        args << QStringLiteral("-init_hw_device")
             << QStringLiteral("vaapi=va:%1").arg(candidate.vaapiDevicePath)
             << QStringLiteral("-filter_hw_device")
             << QStringLiteral("va");
    }
    args << QStringLiteral("-f") << QStringLiteral("lavfi")
         << QStringLiteral("-i")
         << QStringLiteral("color=c=black:s=%1x%2:r=%3:d=%4")
                .arg(probeSize.width())
                .arg(probeSize.height())
                .arg(safeFps)
                .arg(QString::number(probeDurationSeconds, 'f', 3))
         << QStringLiteral("-an")
         << QStringLiteral("-frames:v") << QString::number(probeFrameCount);
    if (candidate.needsVaapiHwUpload) {
        args << QStringLiteral("-vf") << QStringLiteral("format=nv12,hwupload");
    } else {
        args << QStringLiteral("-pix_fmt") << QStringLiteral("yuv420p");
    }
    args << QStringLiteral("-c:v") << candidate.codec;
    if (candidate.explicitBframes >= 0) {
        args << QStringLiteral("-bf") << QString::number(candidate.explicitBframes);
    }
    args << candidate.extraArgs;
    args << QStringLiteral("-f") << QStringLiteral("null") << QStringLiteral("-");

    probe.start(ffmpegPath, args, QIODevice::ReadOnly);
    if (!probe.waitForStarted(3000)) {
        if (detail != nullptr) {
            *detail = QStringLiteral("start_failed:%1").arg(probe.errorString());
        }
        return false;
    }

    constexpr int probeTimeoutMs = 6000;
    probe.waitForFinished(probeTimeoutMs);
    if (probe.state() != QProcess::NotRunning) {
        probe.kill();
        probe.waitForFinished(1000);
        if (detail != nullptr) {
            *detail = QStringLiteral("probe=%1x%2 frames=%3 timeout")
                .arg(probeSize.width())
                .arg(probeSize.height())
                .arg(probeFrameCount);
        }
        return false;
    }

    const QString output = QString::fromUtf8(probe.readAllStandardOutput()).trimmed();
    const bool ok = probe.exitStatus() == QProcess::NormalExit && probe.exitCode() == 0;
    if (detail != nullptr) {
        *detail = QStringLiteral("probe=%1x%2 frames=%3")
            .arg(probeSize.width())
            .arg(probeSize.height())
            .arg(probeFrameCount);
    }
    if (!ok && detail != nullptr) {
        *detail += QStringLiteral(" status=%1 code=%2 output=%3")
                       .arg(static_cast<int>(probe.exitStatus()))
                       .arg(probe.exitCode())
                       .arg(truncateForLog(output, 280));
    }
    return ok;
}

VideoEncoderConfig chooseVideoEncoder(
    const QString& ffmpegPath,
    int outputWidth,
    int outputHeight,
    int fps,
    const ExportRuntimeConfig& exportConfig,
    QString* probeLog)
{
    QProcess query;
    query.setProcessChannelMode(QProcess::MergedChannels);
    query.start(ffmpegPath, {QStringLiteral("-hide_banner"), QStringLiteral("-encoders")}, QIODevice::ReadOnly);
    if (!query.waitForStarted(5000)) {
        if (probeLog) *probeLog = QStringLiteral("encoder_query_start_failed error=%1").arg(query.errorString());
        return {};
    }
    if (!query.waitForFinished(10000)) {
        query.kill();
        query.waitForFinished(1000);
        if (probeLog) *probeLog = QStringLiteral("encoder_query_timeout");
        return {};
    }
    if (query.exitStatus() != QProcess::NormalExit || query.exitCode() != 0) {
        if (probeLog) *probeLog = QStringLiteral("encoder_query_failed output=%1")
            .arg(truncateForLog(QString::fromUtf8(query.readAllStandardOutput()), 1000));
        return {};
    }
    const QString available = QString::fromUtf8(query.readAllStandardOutput());
    const QStringList hardwareCodecs{
        QStringLiteral("h264_videotoolbox"), QStringLiteral("h264_vaapi"),
        QStringLiteral("h264_nvenc"), QStringLiteral("h264_qsv"),
        QStringLiteral("h264_amf"), QStringLiteral("h264_mf"),
        QStringLiteral("hevc_videotoolbox"), QStringLiteral("hevc_vaapi"),
        QStringLiteral("hevc_nvenc"), QStringLiteral("hevc_qsv"),
        QStringLiteral("hevc_amf"), QStringLiteral("hevc_mf")
    };
    const auto makeCandidate = [](const QString& codec, bool hardware) {
        VideoEncoderConfig candidate;
        candidate.codec = codec;
        candidate.isHardware = hardware;
        candidate.explicitBframes = codec.startsWith(QStringLiteral("h264")) ? 0 : -1;
        if (codec.endsWith(QLatin1String("_videotoolbox"))) {
            candidate.extraArgs << QStringLiteral("-allow_sw") << QStringLiteral("0");
        }
        if (codec.endsWith(QLatin1String("_mf"))) {
            candidate.extraArgs << QStringLiteral("-hw_encoding") << QStringLiteral("1");
        }
        if (codec.endsWith(QLatin1String("_vaapi"))) {
            candidate.needsVaapiHwUpload = true;
            candidate.vaapiDevicePath = resolveVaapiDevicePath();
        }
        return candidate;
    };
    QStringList attempts;
    const auto selected = [&](const VideoEncoderConfig& candidate, const QString& selection) {
        if (probeLog) {
            *probeLog = QStringLiteral("encoder_probe selected=%1 hw=%2 selection=%3 size=%4x%5 runtime=%6")
                .arg(candidate.codec).arg(candidate.isHardware ? 1 : 0).arg(selection)
                .arg(outputWidth).arg(outputHeight)
                .arg(truncateForLog(attempts.join(QLatin1Char(';')), 2400));
        }
        return candidate;
    };

    // An explicit diagnostic override selects one codec without automatic routing.
    const QString forced = exportConfig.forcedEncoder.toLower();
    if (!forced.isEmpty()) {
        const bool hardware = hardwareCodecs.contains(forced);
        const bool supported = hardware || forced == QLatin1String("libx264")
            || forced == QLatin1String("libopenh264") || forced == QLatin1String("mpeg4");
        if (!supported || !hasEncoderToken(available, forced)) {
            if (probeLog) *probeLog = QStringLiteral("encoder_probe forced=%1 unavailable").arg(forced);
            return {};
        }
        return selected(makeCandidate(forced, hardware), QStringLiteral("explicit"));
    }

    QStringList hardwareCandidates;
    for (const QString& codec : hardwareCodecs) {
        if (hasEncoderToken(available, codec)) hardwareCandidates.append(codec);
    }
    const int preferred = hardwareCandidates.indexOf(preferredHardwareEncoder().toLower());
    if (preferred > 0) hardwareCandidates.move(preferred, 0);
    for (const QString& codec : hardwareCandidates) {
        const auto candidate = makeCandidate(codec, true);
        QString detail;
        if (exportConfig.skipEncoderRuntimeProbe
            || probeHardwareEncoderRuntime(ffmpegPath, candidate, outputWidth, outputHeight, fps, &detail)) {
            rememberPreferredHardwareEncoder(codec);
            attempts.append(QStringLiteral("%1:%2").arg(codec, exportConfig.skipEncoderRuntimeProbe
                ? QStringLiteral("probe_skipped") : QStringLiteral("ready")));
            return selected(candidate, QStringLiteral("hardware"));
        }
        attempts.append(QStringLiteral("%1:unavailable(%2)").arg(codec, truncateForLog(detail, 180)));
    }

    // Software encoding is reached after every hardware candidate is unavailable.
    for (const QString& codec : {QStringLiteral("libx264"), QStringLiteral("libopenh264"), QStringLiteral("mpeg4")}) {
        if (hasEncoderToken(available, codec)) {
            return selected(makeCandidate(codec, false), QStringLiteral("software_fallback"));
        }
    }
    if (probeLog) *probeLog = QStringLiteral("encoder_probe no_available_encoder runtime=%1")
        .arg(truncateForLog(attempts.join(QLatin1Char(';')), 2400));
    return {};
}

}  // namespace miacode::video_export::detail
