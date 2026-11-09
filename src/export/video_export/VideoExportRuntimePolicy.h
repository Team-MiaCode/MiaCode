#pragma once

#include <optional>

#include <QString>
#include <QtGlobal>

namespace miacode::video_export {

enum class VideoExportSizePreset {
    Standard,
    Compact,
    UltraCompactWithPv,
    UltraCompact,
    Target30Mb,
    Target100Mb,
    Target200Mb,
    Custom,
};

constexpr int kVideoExportWorkerMaxCrashRetries = 1;
constexpr double kCustomVideoBitrateMinMbps = 0.1;
constexpr double kCustomVideoBitrateMaxMbps = 100.0;
constexpr double kCustomVideoBitrateDefaultMbps = 8.0;
inline constexpr auto kVideoExportSizeCorrectionProgress = "Adjusting video bitrate to fit the size limit...";

struct VideoExportSizePolicy {
    double bitrateCoefficient = 0.0;
    qint64 minBitrateKbps = 0;
    qint64 maxBitrateKbps = 0;
    double maxRateMultiplier = 1.0;
    double bufferMultiplier = 1.0;
    int gopSeconds = 2;
    int maxAudioBitrateKbps = 320;
    // A size preset adjusts the quality tier selected by Fast/HighQuality;
    // it never replaces that tier's base CRF.
    int x264CrfAdjustment = 0;
    bool disableVideoBackground = false;
    bool usePeakConstrainedVbr = false;
};

VideoExportSizePolicy videoExportSizePolicy(VideoExportSizePreset preset);
QString videoExportSizePresetToken(VideoExportSizePreset preset);
VideoExportSizePreset videoExportSizePresetFromToken(const QString& token,
    VideoExportSizePreset fallback = VideoExportSizePreset::Standard);
double normalizedCustomVideoBitrateMbps(double value);
qint64 videoExportTargetBytes(VideoExportSizePreset preset);
qint64 videoExportTargetBitrateKbps(VideoExportSizePreset preset, double seconds, int audioKbps);
int effectiveVideoExportAudioBitrateKbps(VideoExportSizePreset preset, int requestedKbps);
int effectiveVideoExportX264Crf(VideoExportSizePreset preset, int baseCrf);

bool shouldRequestOffscreenPboReadback(
    const std::optional<bool>& enableOverride,
    const std::optional<bool>& disableOverride
);

bool shouldRetryVideoExportWorkerAfterCrash(
    bool crashExit,
    bool pboRequestedForAttempt,
    bool cancelRequested,
    int completedAttemptCount
);

}  // namespace miacode::video_export
