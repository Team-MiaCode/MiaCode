#include "export/video_export/VideoExportRuntimePolicy.h"

#include <cmath>

namespace miacode::video_export {

VideoExportSizePolicy videoExportSizePolicy(VideoExportSizePreset preset)
{
    VideoExportSizePolicy policy;
    switch (preset) {
    case VideoExportSizePreset::Target30Mb:
        policy.maxAudioBitrateKbps = 128;
        return policy;
    case VideoExportSizePreset::Target100Mb:
    case VideoExportSizePreset::Target200Mb:
        policy.maxAudioBitrateKbps = 192;
        return policy;
    case VideoExportSizePreset::Custom:
        return policy;
    case VideoExportSizePreset::Compact:
        policy.bitrateCoefficient = 0.070;
        policy.minBitrateKbps = 1800;
        policy.maxBitrateKbps = 8000;
        policy.maxRateMultiplier = 1.35;
        policy.bufferMultiplier = 2.0;
        policy.gopSeconds = 4;
        policy.maxAudioBitrateKbps = 160;
        policy.x264CrfAdjustment = 1;
        policy.usePeakConstrainedVbr = true;
        return policy;
    case VideoExportSizePreset::UltraCompactWithPv:
    case VideoExportSizePreset::UltraCompact:
        policy.bitrateCoefficient = 0.035;
        policy.minBitrateKbps = 4000;
        policy.maxBitrateKbps = 4000;
        policy.maxRateMultiplier = 1.0;
        policy.bufferMultiplier = 2.0;
        policy.gopSeconds = 6;
        policy.maxAudioBitrateKbps = 128;
        policy.x264CrfAdjustment = 3;
        policy.disableVideoBackground = preset == VideoExportSizePreset::UltraCompact;
        policy.usePeakConstrainedVbr = true;
        return policy;
    case VideoExportSizePreset::Standard:
    default:
        return policy;
    }
}

QString videoExportSizePresetToken(VideoExportSizePreset preset)
{
    switch (preset) {
    case VideoExportSizePreset::Target30Mb:
        return QStringLiteral("target_30mb");
    case VideoExportSizePreset::Target100Mb:
        return QStringLiteral("target_100mb");
    case VideoExportSizePreset::Target200Mb:
        return QStringLiteral("target_200mb");
    case VideoExportSizePreset::Custom:
        return QStringLiteral("custom");
    case VideoExportSizePreset::Compact:
        return QStringLiteral("compact");
    case VideoExportSizePreset::UltraCompact:
        return QStringLiteral("ultra_compact");
    case VideoExportSizePreset::UltraCompactWithPv:
        return QStringLiteral("ultra_compact_with_pv");
    case VideoExportSizePreset::Standard:
    default:
        return QStringLiteral("standard");
    }
}

VideoExportSizePreset videoExportSizePresetFromToken(const QString& token, VideoExportSizePreset fallback)
{
    const QString normalized = token.trimmed().toLower();
    for (const auto preset : {VideoExportSizePreset::Standard, VideoExportSizePreset::Compact,
            VideoExportSizePreset::UltraCompactWithPv, VideoExportSizePreset::UltraCompact,
            VideoExportSizePreset::Target30Mb, VideoExportSizePreset::Target100Mb,
            VideoExportSizePreset::Target200Mb, VideoExportSizePreset::Custom}) {
        if (normalized == videoExportSizePresetToken(preset)) {
            return preset;
        }
    }
    return fallback;
}

double normalizedCustomVideoBitrateMbps(double value)
{
    return std::isfinite(value)
        ? qBound(kCustomVideoBitrateMinMbps, value, kCustomVideoBitrateMaxMbps)
        : kCustomVideoBitrateDefaultMbps;
}

qint64 videoExportTargetBytes(VideoExportSizePreset preset)
{
    switch (preset) {
    case VideoExportSizePreset::Target30Mb: return 30'000'000;
    case VideoExportSizePreset::Target100Mb: return 100'000'000;
    case VideoExportSizePreset::Target200Mb: return 200'000'000;
    default: return 0;
    }
}

qint64 videoExportTargetBitrateKbps(VideoExportSizePreset preset, double seconds, int audioKbps)
{
    const qint64 bytes = videoExportTargetBytes(preset);
    if (bytes == 0 || !std::isfinite(seconds) || seconds <= 0.0) {
        return 0;
    }
    // MB is decimal. Reserve space for the MP4 container and the AAC stream.
    const double totalKbps = static_cast<double>(bytes) * 0.975 * 8 / (1000 * seconds);
    return static_cast<qint64>(qBound(0.0, totalKbps - audioKbps - 16.0, 100'000.0));
}

int effectiveVideoExportAudioBitrateKbps(VideoExportSizePreset preset, int requestedKbps)
{
    const VideoExportSizePolicy policy = videoExportSizePolicy(preset);
    return qBound(96, requestedKbps, policy.maxAudioBitrateKbps);
}

int effectiveVideoExportX264Crf(VideoExportSizePreset preset, int baseCrf)
{
    const VideoExportSizePolicy policy = videoExportSizePolicy(preset);
    return qBound(16, baseCrf + policy.x264CrfAdjustment, 28);
}

bool shouldRequestOffscreenPboReadback(
    const std::optional<bool>& enableOverride,
    const std::optional<bool>& disableOverride
)
{
    if (disableOverride.value_or(false)) {
        return false;
    }
    if (enableOverride.has_value()) {
        return enableOverride.value();
    }
    return true;
}

bool shouldRetryVideoExportWorkerAfterCrash(
    bool crashExit,
    bool pboRequestedForAttempt,
    bool cancelRequested,
    int completedAttemptCount
)
{
    return crashExit
        && pboRequestedForAttempt
        && !cancelRequested
        && completedAttemptCount <= kVideoExportWorkerMaxCrashRetries;
}

}  // namespace miacode::video_export
