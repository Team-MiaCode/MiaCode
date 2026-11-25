#include "export/video_export/VideoExportEncoding.h"

#include "audio/PreviewAudioMixConfig.h"
#include "export/video_export/EncoderProbeCache.h"

#include <QtMath>

#include <cmath>

namespace miacode::video_export {
namespace {

// Average bits per pixel per frame of each quality level (high, standard, small)
// with the settings in VideoExportEncoderProfiles.cpp. Measured 2026-11-13 on 30 s
// chart exports at 1080x1080@60 with a PV and a still background, at VMAF about
// 97.5 / 94.5 / 89. Encoders without their own row use the libx264 row.
constexpr double kX264BitsPerPixel[] = {0.066, 0.040, 0.0275};
constexpr double kAmfBitsPerPixel[] = {0.114, 0.077, 0.052};
constexpr double kMediaFoundationBitsPerPixel[] = {0.185, 0.125, 0.070};

constexpr qint64 kMp4FixedOverheadBytes = 64 * 1024;
// Sample size, chunk offset and sync entries per sample, rounded up.
constexpr qint64 kMp4BytesPerSample = 12;
constexpr int kAacSamplesPerFrame = 1024;

qint64 bytesForKbps(qint64 kbps, double seconds)
{
    return qRound64(static_cast<double>(kbps) * 125.0 * seconds);
}

}  // namespace

VideoEncodingSettings normalizedVideoEncodingSettings(VideoEncodingSettings settings)
{
    settings.sizeLimitMb = qBound(kVideoSizeLimitMinMb, settings.sizeLimitMb, kVideoSizeLimitMaxMb);
    settings.videoBitrateKbps = qBound(kVideoBitrateMinKbps, settings.videoBitrateKbps, kVideoBitrateMaxKbps);
    return settings;
}

QString videoRateModeToken(VideoRateMode mode)
{
    switch (mode) {
    case VideoRateMode::FileSize:
        return QStringLiteral("file_size");
    case VideoRateMode::Bitrate:
        return QStringLiteral("bitrate");
    case VideoRateMode::Quality:
    default:
        return QStringLiteral("quality");
    }
}

VideoRateMode videoRateModeFromToken(const QString& token, VideoRateMode fallback)
{
    for (const VideoRateMode mode : {VideoRateMode::Quality, VideoRateMode::FileSize, VideoRateMode::Bitrate}) {
        if (token.compare(videoRateModeToken(mode), Qt::CaseInsensitive) == 0) {
            return mode;
        }
    }
    return fallback;
}

QString videoQualityLevelToken(VideoQualityLevel level)
{
    switch (level) {
    case VideoQualityLevel::High:
        return QStringLiteral("high");
    case VideoQualityLevel::Small:
        return QStringLiteral("small");
    case VideoQualityLevel::Standard:
    default:
        return QStringLiteral("standard");
    }
}

VideoQualityLevel videoQualityLevelFromToken(const QString& token, VideoQualityLevel fallback)
{
    for (const VideoQualityLevel level :
         {VideoQualityLevel::High, VideoQualityLevel::Standard, VideoQualityLevel::Small}) {
        if (token.compare(videoQualityLevelToken(level), Qt::CaseInsensitive) == 0) {
            return level;
        }
    }
    return fallback;
}

QString videoRateControlToken(VideoRateControl control)
{
    switch (control) {
    case VideoRateControl::AverageBitrate:
        return QStringLiteral("abr");
    case VideoRateControl::CappedBitrate:
        return QStringLiteral("capped");
    case VideoRateControl::ConstantQuality:
    default:
        return QStringLiteral("cq");
    }
}

qint64 qualityLevelReferenceKbps(
    VideoQualityLevel level, int width, int height, int fps, const QString& encoder)
{
    const double* table = encoder.endsWith(QLatin1String("_amf"))
        ? kAmfBitsPerPixel
        : (encoder.endsWith(QLatin1String("_mf")) ? kMediaFoundationBitsPerPixel : kX264BitsPerPixel);
    const double bitsPerPixel = table[static_cast<int>(level)];
    const double pixelsPerSecond =
        static_cast<double>(qMax(1, width)) * qMax(1, height) * qMax(1, fps);
    return qMax<qint64>(1, qRound64(bitsPerPixel * pixelsPerSecond / 1000.0));
}

QString plannedVideoEncoder()
{
    const auto cache = encoderProbeCache();
    const QString preferred = cache ? cache->preferredHardwareEncoder().trimmed().toLower() : QString();
    return preferred.isEmpty() ? QStringLiteral("libx264") : preferred;
}

qint64 estimatedMp4OverheadBytes(int videoFrameCount, double seconds)
{
    const double audioFrames = std::ceil(
        qMax(0.0, seconds) * miacode::preview_audio::kMixSampleRate / kAacSamplesPerFrame);
    return kMp4FixedOverheadBytes
        + kMp4BytesPerSample * (qMax(0, videoFrameCount) + static_cast<qint64>(audioFrames));
}

VideoRatePlan planVideoRate(const VideoEncodingSettings& requested, const VideoRateInputs& inputs)
{
    const VideoEncodingSettings settings = normalizedVideoEncodingSettings(requested);
    const double seconds = qMax(0.001, inputs.seconds);
    const qint64 audioBytes = inputs.audioBytes >= 0 ? inputs.audioBytes : bytesForKbps(inputs.audioKbps, seconds);
    const qint64 overheadBytes = estimatedMp4OverheadBytes(inputs.videoFrameCount, seconds);

    VideoRatePlan plan;
    plan.mode = settings.mode;
    plan.quality = settings.quality;
    plan.audioKbps = inputs.audioKbps;
    switch (settings.mode) {
    case VideoRateMode::FileSize: {
        plan.control = VideoRateControl::CappedBitrate;
        plan.sizeLimitBytes = static_cast<qint64>(settings.sizeLimitMb) * 1000 * 1000;
        // With maxrate = bufsize = T the encoder writes at most T x (seconds + 1) bits.
        const qint64 videoBytes = plan.sizeLimitBytes - audioBytes - overheadBytes;
        plan.videoKbps = videoBytes > 0
            ? static_cast<qint64>(std::floor(static_cast<double>(videoBytes) * 8.0 / 1000.0 / (seconds + 1.0)))
            : 0;
        plan.maxRateKbps = plan.videoKbps;
        plan.bufSizeKbps = plan.videoKbps;
        plan.valid = plan.videoKbps >= kVideoBudgetMinKbps;
        break;
    }
    case VideoRateMode::Bitrate:
        plan.control = VideoRateControl::AverageBitrate;
        plan.videoKbps = settings.videoBitrateKbps;
        plan.maxRateKbps = qRound64(plan.videoKbps * 1.5);
        plan.bufSizeKbps = plan.videoKbps * 2;
        plan.valid = true;
        break;
    case VideoRateMode::Quality:
    default:
        plan.control = VideoRateControl::ConstantQuality;
        plan.videoKbps = qualityLevelReferenceKbps(
            settings.quality, inputs.width, inputs.height, inputs.fps, inputs.encoder);
        plan.maxRateKbps = qRound64(plan.videoKbps * 1.5);
        plan.bufSizeKbps = plan.videoKbps * 2;
        plan.valid = true;
        break;
    }
    if (plan.valid) {
        plan.estimatedBytes = bytesForKbps(plan.videoKbps, seconds) + audioBytes + overheadBytes;
    }
    return plan;
}

}  // namespace miacode::video_export
