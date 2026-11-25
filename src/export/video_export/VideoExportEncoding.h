#pragma once

#include <QString>
#include <QtGlobal>

namespace miacode::video_export {

enum class VideoRateMode {
    Quality,
    FileSize,
    Bitrate,
};

enum class VideoQualityLevel {
    High,
    Standard,
    Small,
};

inline constexpr int kVideoSizeLimitMinMb = 5;
inline constexpr int kVideoSizeLimitMaxMb = 1000;
inline constexpr int kVideoSizeLimitDefaultMb = 100;
inline constexpr int kVideoBitrateMinKbps = 100;
inline constexpr int kVideoBitrateMaxKbps = 100000;
inline constexpr int kVideoBitrateDefaultKbps = 8000;
// Below this the picture is unusable; a size limit that leaves less is rejected.
inline constexpr qint64 kVideoBudgetMinKbps = 64;
inline constexpr int kVideoGopSeconds = 2;
// Worker failure sentence; the app maps it to a localized message.
inline constexpr auto kVideoExportSizeBudgetTooSmallMessage =
    "The size limit cannot hold the audio of this export.";

// The export page's rate-control choice. Every video encoder argument is derived
// from this and nothing else, so what the page shows is what the encoder gets.
struct VideoEncodingSettings {
    VideoRateMode mode = VideoRateMode::Quality;
    VideoQualityLevel quality = VideoQualityLevel::Standard;
    int sizeLimitMb = kVideoSizeLimitDefaultMb;  // decimal megabytes
    int videoBitrateKbps = kVideoBitrateDefaultKbps;

    bool operator==(const VideoEncodingSettings&) const = default;
};

VideoEncodingSettings normalizedVideoEncodingSettings(VideoEncodingSettings settings);

QString videoRateModeToken(VideoRateMode mode);
VideoRateMode videoRateModeFromToken(const QString& token, VideoRateMode fallback);
QString videoQualityLevelToken(VideoQualityLevel level);
VideoQualityLevel videoQualityLevelFromToken(const QString& token, VideoQualityLevel fallback);

enum class VideoRateControl {
    ConstantQuality,
    AverageBitrate,
    // Average bitrate whose VBV bound (maxrate x duration + bufsize) fits the size limit.
    CappedBitrate,
};

QString videoRateControlToken(VideoRateControl control);

struct VideoRateInputs {
    int width = 0;
    int height = 0;
    int fps = 0;
    int videoFrameCount = 0;
    double seconds = 0.0;
    int audioKbps = 0;
    // Exact size of the encoded audio stream once known; otherwise it is estimated
    // from audioKbps.
    qint64 audioBytes = -1;
    // Encoder the quality-mode estimate assumes; see plannedVideoEncoder().
    QString encoder;
};

struct VideoRatePlan {
    bool valid = false;
    VideoRateMode mode = VideoRateMode::Quality;
    VideoQualityLevel quality = VideoQualityLevel::Standard;
    VideoRateControl control = VideoRateControl::ConstantQuality;
    // Average-bitrate target. In quality mode this is the quality level's measured
    // rate on inputs.encoder, used for the size estimate and as the fallback when an
    // encoder's constant-quality mode is unavailable.
    qint64 videoKbps = 0;
    qint64 maxRateKbps = 0;
    qint64 bufSizeKbps = 0;
    int audioKbps = 0;
    qint64 sizeLimitBytes = 0;
    qint64 estimatedBytes = 0;
};

// The single rate computation shared by the export page estimate and the encoder.
VideoRatePlan planVideoRate(const VideoEncodingSettings& settings, const VideoRateInputs& inputs);

// Average bitrate a quality level produces on `encoder` at this frame size and rate.
qint64 qualityLevelReferenceKbps(
    VideoQualityLevel level, int width, int height, int fps, const QString& encoder);

// The encoder an export is expected to use: the last hardware encoder that
// worked on this machine, else libx264. The page and the worker both plan with it.
QString plannedVideoEncoder();

// MP4 index and header bytes for this many video samples plus the AAC samples of
// `seconds` of audio.
qint64 estimatedMp4OverheadBytes(int videoFrameCount, double seconds);

}  // namespace miacode::video_export
