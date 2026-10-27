#include "export/video_export/VideoExportControllerInternal.h"

namespace miacode::video_export::detail {

QString fasterX264Preset(const QString& preset)
{
    if (preset == QLatin1String("medium")) {
        return QStringLiteral("fast");
    }
    if (preset == QLatin1String("fast")) {
        return QStringLiteral("faster");
    }
    if (preset == QLatin1String("faster")) {
        return QStringLiteral("veryfast");
    }
    if (preset == QLatin1String("veryfast")) {
        return QStringLiteral("superfast");
    }
    return preset;
}

QString videoExportPresetToken(VideoExportPreset preset)
{
    switch (preset) {
    case VideoExportPreset::HighQuality:
        return QStringLiteral("high_quality");
    case VideoExportPreset::Fast:
    default:
        return QStringLiteral("fast");
    }
}

VideoBitratePlan chooseVideoBitratePlan(
    VideoExportPreset preset,
    VideoExportSizePreset sizePreset,
    int outputWidth,
    int outputHeight,
    int fps
)
{
    const int safeWidth = qMax(1, outputWidth);
    const int safeHeight = qMax(1, outputHeight);
    const int safeFps = qMax(1, fps);

    VideoBitratePlan plan;
    const miacode::video_export::VideoExportSizePolicy sizePolicy =
        miacode::video_export::videoExportSizePolicy(sizePreset);
    if (sizePreset != VideoExportSizePreset::Standard) {
        plan.bitrateKbps = qBound<qint64>(
            sizePolicy.minBitrateKbps,
            qRound64(static_cast<double>(safeWidth) * safeHeight * safeFps
                     * sizePolicy.bitrateCoefficient / 1000.0),
            sizePolicy.maxBitrateKbps
        );
        plan.maxRateKbps = qMax<qint64>(
            plan.bitrateKbps,
            qRound64(static_cast<double>(plan.bitrateKbps) * sizePolicy.maxRateMultiplier)
        );
        plan.bufSizeKbps = qMax<qint64>(
            plan.maxRateKbps,
            qRound64(static_cast<double>(plan.maxRateKbps) * sizePolicy.bufferMultiplier)
        );
        return plan;
    }
    if (preset == VideoExportPreset::HighQuality) {
        plan.bitrateKbps = qBound<qint64>(
            2600LL,
            qRound64(static_cast<double>(safeWidth) * safeHeight * safeFps * 0.090 / 1000.0),
            10500LL
        );
        plan.maxRateKbps = qBound<qint64>(
            plan.bitrateKbps,
            qRound64(static_cast<double>(plan.bitrateKbps) * 1.35),
            14000LL
        );
        plan.bufSizeKbps = qBound<qint64>(
            plan.maxRateKbps,
            qRound64(static_cast<double>(plan.maxRateKbps) * 1.75),
            20000LL
        );
        return plan;
    }

    plan.bitrateKbps = qBound<qint64>(
        2200LL,
        qRound64(static_cast<double>(safeWidth) * safeHeight * safeFps * 0.075 / 1000.0),
        8500LL
    );
    plan.maxRateKbps = qBound<qint64>(
        plan.bitrateKbps,
        qRound64(static_cast<double>(plan.bitrateKbps) * 1.40),
        10500LL
    );
    plan.bufSizeKbps = qBound<qint64>(
        plan.maxRateKbps,
        qRound64(static_cast<double>(plan.maxRateKbps) * 2.0),
        16000LL
    );
    return plan;
}

X264TuningPlan chooseX264TuningPlan(
    VideoExportPreset preset,
    VideoExportSizePreset sizePreset,
    const ExportRuntimeConfig& exportConfig,
    const SystemMemoryInfo& memoryInfo,
    int outputWidth,
    int outputHeight,
    int fps,
    int idealThreadCount
)
{
    const qint64 availMiB = bytesToMiB(memoryInfo.availablePhysicalBytes);
    const qint64 totalMiB = bytesToMiB(memoryInfo.totalPhysicalBytes);
    const qint64 pixelsPerSecond =
        static_cast<qint64>(qMax(1, outputWidth)) * qMax(1, outputHeight) * qMax(1, fps);

    X264TuningPlan plan;
    if (preset == VideoExportPreset::Fast) {
        plan.preset = QStringLiteral("veryfast");
        plan.crf = 22;
        plan.bframes = 0;
        plan.tune = QStringLiteral("animation");
        if (!exportConfig.x264PresetOverride.isEmpty()) {
            plan.preset = exportConfig.x264PresetOverride;
        }
        plan.crf = qBound(
            16,
            exportConfig.x264CrfOverride >= 0
                ? exportConfig.x264CrfOverride
                : miacode::video_export::effectiveVideoExportX264Crf(sizePreset, plan.crf),
            28);
        plan.bframes = qBound(
            0,
            exportConfig.x264BframesOverride >= 0 ? exportConfig.x264BframesOverride : plan.bframes,
            8);
        return plan;
    }
    if (memoryInfo.valid) {
        if (availMiB >= 24576 && totalMiB >= 32768) {
            plan.preset = QStringLiteral("medium");
        } else if (availMiB >= 12288) {
            plan.preset = QStringLiteral("fast");
        } else {
            plan.preset = QStringLiteral("faster");
        }
    }

    if (idealThreadCount <= 4) {
        plan.preset = QStringLiteral("faster");
    } else if (idealThreadCount <= 8 && plan.preset == QLatin1String("medium")) {
        plan.preset = QStringLiteral("fast");
    } else if (idealThreadCount <= 8 && plan.preset == QLatin1String("fast") && memoryInfo.valid && availMiB < 16384) {
        plan.preset = QStringLiteral("faster");
    }

    if (memoryInfo.valid && availMiB < 8192) {
        plan.preset = QStringLiteral("faster");
    } else if (memoryInfo.valid && availMiB < 12288 && plan.preset == QLatin1String("medium")) {
        plan.preset = QStringLiteral("fast");
    }

    if (memoryInfo.valid && totalMiB < 16384 && plan.preset == QLatin1String("medium")) {
        plan.preset = QStringLiteral("fast");
    }

    if (pixelsPerSecond >= 120000000LL) {
        plan.preset = fasterX264Preset(plan.preset);
    }
    if (pixelsPerSecond >= 180000000LL) {
        plan.preset = fasterX264Preset(plan.preset);
    }

    // Compactness bump. After the memory/thread/pixel-rate logic above
    // settles on a preset, advance the two cheap ones by a notch:
    //   faster → fast  (~10-15% smaller, ~30% slower encode)
    //   fast   → medium (~10-12% smaller, ~50-100% slower encode)
    // Higher presets (medium, slow, slower, veryslow) and the
    // ultrafast/superfast/veryfast tail are left alone — those slots
    // are picked by other branches deliberately and bumping them costs
    // disproportionate encode time per byte saved.
    if (plan.preset == QLatin1String("faster")) {
        plan.preset = QStringLiteral("fast");
    } else if (plan.preset == QLatin1String("fast")) {
        plan.preset = QStringLiteral("medium");
    }
    plan.tune = QStringLiteral("animation");

    if (preset == VideoExportPreset::HighQuality) {
        // Pull two CRF notches below the Fast default. With Fast at
        // crf=22 this lands HighQuality at crf=20 — perceptually
        // transparent on the high-contrast UI overlay and still well
        // above the qMax(18, …) floor, which keeps the file size from
        // exploding if some future tweak lowers the Fast default.
        plan.crf = qMax(18, plan.crf - 2);
        plan.bframes = 0;
    }

    if (!exportConfig.x264PresetOverride.isEmpty()) {
        plan.preset = exportConfig.x264PresetOverride;
    }
    plan.crf = qBound(
        16,
        exportConfig.x264CrfOverride >= 0
            ? exportConfig.x264CrfOverride
            : miacode::video_export::effectiveVideoExportX264Crf(sizePreset, plan.crf),
        28
    );
    // beta25 — bframes upper bound raised from 2 to 8 to honour the new
    // x264 stock default of 3 (and let advanced users push higher via
    // MIACODE_EXPORT_X264_BFRAMES). 8 is x264's recommended ceiling for
    // typical content; values above that hit diminishing returns and
    // can slow decode on weaker playback hardware.
    plan.bframes = qBound(
        0,
        exportConfig.x264BframesOverride >= 0 ? exportConfig.x264BframesOverride : plan.bframes,
        8
    );
    return plan;
}


void applyVideoEncodingParameters(
    VideoEncoderConfig* encoder,
    VideoExportPreset preset,
    VideoExportSizePreset sizePreset,
    int outputWidth,
    int outputHeight,
    int fps,
    const SystemMemoryInfo& memoryInfo,
    const ExportRuntimeConfig& exportConfig)
{
    const VideoBitratePlan bitrate = chooseVideoBitratePlan(preset, sizePreset, outputWidth, outputHeight, fps);
    const auto sizePolicy = miacode::video_export::videoExportSizePolicy(sizePreset);
    const QStringList bitrateArgs = bitrate.toArgs();
    if (encoder->codec == QLatin1String("libx264")) {
        const auto tuning = chooseX264TuningPlan(preset, sizePreset, exportConfig, memoryInfo,
            outputWidth, outputHeight, fps, qMax(1, QThread::idealThreadCount()));
        encoder->extraArgs << tuning.toArgs();
        if (sizePreset != VideoExportSizePreset::Standard) encoder->extraArgs << bitrateArgs;
        return;
    }
    if (encoder->codec == QLatin1String("libopenh264")) {
        qint64 target = bitrate.bitrateKbps;
        if (sizePreset == VideoExportSizePreset::Standard && preset == VideoExportPreset::HighQuality) {
            target = qRound64(target * 1.10);
        }
        encoder->extraArgs << QStringLiteral("-b:v") << QString::number(qMax<qint64>(600, target)) + QLatin1String("k");
        return;
    }
    if (encoder->codec == QLatin1String("mpeg4") && sizePreset == VideoExportSizePreset::Standard) {
        encoder->extraArgs << QStringLiteral("-q:v")
                           << (preset == VideoExportPreset::HighQuality ? QStringLiteral("3") : QStringLiteral("4"));
        return;
    }
    encoder->extraArgs << bitrateArgs;
    if (encoder->codec == QLatin1String("h264_mf") && sizePolicy.usePeakConstrainedVbr) {
        encoder->extraArgs << QStringLiteral("-rate_control") << QStringLiteral("pc_vbr")
                           << QStringLiteral("-scenario") << QStringLiteral("archive");
    }
}

} // namespace miacode::video_export::detail
