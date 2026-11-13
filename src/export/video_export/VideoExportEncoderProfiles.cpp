#include "export/video_export/VideoExportControllerInternal.h"

// VideoExportEncoderProfiles.cpp — the one place that knows how each encoder
// expresses a rate plan. Every option name here is checked against the bundled
// FFmpeg (`ffmpeg -h encoder=<name>`). Export speed comes first: every rate mode
// takes the first hardware encoder that passes the runtime probe.
//
// Quality levels aim at VMAF about 97.5 / 94.5 / 89 (high / standard / small).
// libx264, AMF and Media Foundation were measured on 2026-11-13 (30 s chart
// exports, 1080x1080@60, PV and still backgrounds); NVENC, QSV, VAAPI,
// VideoToolbox and mpeg4 follow the libx264 scale until measured on their hardware.

namespace miacode::video_export::detail {
namespace {

enum class EncoderFamily {
    X264,
    Nvenc,
    Qsv,
    Amf,
    MediaFoundation,
    VideoToolbox,
    Vaapi,
    OpenH264,
    Mpeg4,
};

struct EncoderProfile {
    EncoderFamily family = EncoderFamily::X264;
    bool hardware = false;
    bool hevc = false;
};

std::optional<EncoderProfile> encoderProfile(const QString& codec)
{
    if (codec == QLatin1String("libx264")) {
        return EncoderProfile{EncoderFamily::X264, false, false};
    }
    if (codec == QLatin1String("libopenh264")) {
        return EncoderProfile{EncoderFamily::OpenH264, false, false};
    }
    if (codec == QLatin1String("mpeg4")) {
        return EncoderProfile{EncoderFamily::Mpeg4, false, false};
    }
    const bool hevc = codec.startsWith(QLatin1String("hevc_"));
    if (!hevc && !codec.startsWith(QLatin1String("h264_"))) {
        return std::nullopt;
    }
    const QString backend = codec.section(QLatin1Char('_'), 1);
    if (backend == QLatin1String("nvenc")) {
        return EncoderProfile{EncoderFamily::Nvenc, true, hevc};
    }
    if (backend == QLatin1String("qsv")) {
        return EncoderProfile{EncoderFamily::Qsv, true, hevc};
    }
    if (backend == QLatin1String("amf")) {
        return EncoderProfile{EncoderFamily::Amf, true, hevc};
    }
    if (backend == QLatin1String("mf")) {
        return EncoderProfile{EncoderFamily::MediaFoundation, true, hevc};
    }
    if (backend == QLatin1String("videotoolbox")) {
        return EncoderProfile{EncoderFamily::VideoToolbox, true, hevc};
    }
    if (backend == QLatin1String("vaapi")) {
        return EncoderProfile{EncoderFamily::Vaapi, true, hevc};
    }
    return std::nullopt;
}

QString kbpsArg(qint64 kbps)
{
    return QString::number(kbps) + QLatin1Char('k');
}

QStringList vbvArgs(qint64 rateKbps, qint64 maxRateKbps, qint64 bufSizeKbps)
{
    return {QStringLiteral("-b:v"), kbpsArg(rateKbps),
            QStringLiteral("-maxrate"), kbpsArg(maxRateKbps),
            QStringLiteral("-bufsize"), kbpsArg(bufSizeKbps)};
}

QString pixelFormatFor(EncoderFamily family)
{
    switch (family) {
    case EncoderFamily::Qsv:
    case EncoderFamily::Amf:
    case EncoderFamily::MediaFoundation:
        return QStringLiteral("nv12");
    case EncoderFamily::Vaapi:
        return QString();  // uploaded as nv12 by the hwupload filter
    default:
        return QStringLiteral("yuv420p");
    }
}

// Speed and compatibility switches that do not depend on the rate plan.
QStringList fixedArgs(const EncoderProfile& profile)
{
    QStringList args;
    switch (profile.family) {
    case EncoderFamily::X264:
        args << QStringLiteral("-preset") << QStringLiteral("fast")
             << QStringLiteral("-tune") << QStringLiteral("animation");
        break;
    case EncoderFamily::Nvenc:
        args << QStringLiteral("-preset") << QStringLiteral("p5")
             << QStringLiteral("-tune") << QStringLiteral("hq");
        break;
    case EncoderFamily::Qsv:
        args << QStringLiteral("-preset") << QStringLiteral("medium");
        break;
    case EncoderFamily::Amf:
        args << QStringLiteral("-quality") << QStringLiteral("balanced");
        break;
    case EncoderFamily::MediaFoundation:
        args << QStringLiteral("-hw_encoding") << QStringLiteral("1")
             << QStringLiteral("-scenario") << QStringLiteral("archive");
        break;
    case EncoderFamily::VideoToolbox:
        args << QStringLiteral("-allow_sw") << QStringLiteral("0");
        break;
    case EncoderFamily::Vaapi:
    case EncoderFamily::OpenH264:
    case EncoderFamily::Mpeg4:
        break;
    }
    // B-frames stay off for H.264 so every frame decodes in presentation order.
    const bool h264 = profile.family == EncoderFamily::X264 || (profile.hardware && !profile.hevc);
    if (h264) {
        args << QStringLiteral("-bf") << QStringLiteral("0");
    }
    if (profile.hevc) {
        // Apple players only recognise HEVC in MP4 under the hvc1 tag.
        args << QStringLiteral("-tag:v") << QStringLiteral("hvc1");
    }
    return args;
}

std::optional<QStringList> constantQualityArgs(EncoderFamily family, VideoQualityLevel level)
{
    const int index = static_cast<int>(level);
    const auto pick = [index](int highValue, int standardValue, int smallValue) {
        return QString::number(index == 0 ? highValue : (index == 1 ? standardValue : smallValue));
    };
    switch (family) {
    case EncoderFamily::X264:
        return QStringList{QStringLiteral("-crf"), pick(21, 26, 30)};
    case EncoderFamily::Nvenc:
        return QStringList{QStringLiteral("-rc"), QStringLiteral("vbr"),
                           QStringLiteral("-cq"), pick(21, 26, 30),
                           QStringLiteral("-b:v"), QStringLiteral("0")};
    case EncoderFamily::Qsv:
        return QStringList{QStringLiteral("-global_quality"), pick(21, 26, 30)};
    case EncoderFamily::Amf:
        // Higher QVBR levels mean higher quality, unlike CRF.
        return QStringList{QStringLiteral("-rc"), QStringLiteral("qvbr"),
                           QStringLiteral("-qvbr_quality_level"), pick(50, 38, 28)};
    case EncoderFamily::MediaFoundation:
        return QStringList{QStringLiteral("-rate_control"), QStringLiteral("quality"),
                           QStringLiteral("-quality"), pick(90, 75, 55)};
    case EncoderFamily::VideoToolbox:
        return QStringList{QStringLiteral("-q:v"), pick(75, 60, 45)};
    case EncoderFamily::Vaapi:
        return QStringList{QStringLiteral("-rc_mode"), QStringLiteral("ICQ"),
                           QStringLiteral("-global_quality"), pick(21, 26, 30)};
    case EncoderFamily::Mpeg4:
        return QStringList{QStringLiteral("-q:v"), pick(2, 4, 7)};
    case EncoderFamily::OpenH264:
        return std::nullopt;
    }
    return std::nullopt;
}

std::optional<QStringList> averageBitrateArgs(EncoderFamily family, const VideoRatePlan& plan)
{
    const QStringList vbv = vbvArgs(plan.videoKbps, plan.maxRateKbps, plan.bufSizeKbps);
    switch (family) {
    case EncoderFamily::X264:
    case EncoderFamily::Qsv:
    case EncoderFamily::Mpeg4:
        return vbv;
    case EncoderFamily::Nvenc:
        return QStringList{QStringLiteral("-rc"), QStringLiteral("vbr")} + vbv;
    case EncoderFamily::Amf:
        return QStringList{QStringLiteral("-rc"), QStringLiteral("vbr_peak")} + vbv;
    case EncoderFamily::Vaapi:
        return QStringList{QStringLiteral("-rc_mode"), QStringLiteral("VBR")} + vbv;
    case EncoderFamily::MediaFoundation:
        return QStringList{QStringLiteral("-rate_control"), QStringLiteral("pc_vbr"),
                           QStringLiteral("-b:v"), kbpsArg(plan.videoKbps)};
    case EncoderFamily::VideoToolbox:
        return QStringList{QStringLiteral("-b:v"), kbpsArg(plan.videoKbps),
                           QStringLiteral("-maxrate"), kbpsArg(plan.maxRateKbps)};
    case EncoderFamily::OpenH264:
        return QStringList{QStringLiteral("-b:v"), kbpsArg(plan.videoKbps)};
    }
    return std::nullopt;
}

// A size limit keeps the hardware encoder for speed, so the limit is a target
// rather than a guarantee. libx264 stayed within its VBV bound (maxrate x
// duration + bufsize) at every measured target; AMF CBR stayed within it at
// 1.5 Mbps and above but overshot by 40-54 % at 0.4 Mbps, and Media Foundation
// CBR by 71-81 %, because neither goes below its minimum bitrate.
std::optional<QStringList> cappedBitrateArgs(EncoderFamily family, const VideoRatePlan& plan)
{
    const QStringList vbv = vbvArgs(plan.videoKbps, plan.maxRateKbps, plan.bufSizeKbps);
    switch (family) {
    case EncoderFamily::X264:
    case EncoderFamily::Mpeg4:
    case EncoderFamily::Qsv:  // QSV selects CBR when maxrate equals the bitrate.
        return vbv;
    case EncoderFamily::Nvenc:
        return QStringList{QStringLiteral("-rc"), QStringLiteral("cbr")} + vbv;
    case EncoderFamily::Amf:
        return QStringList{QStringLiteral("-rc"), QStringLiteral("cbr"),
                           QStringLiteral("-enforce_hrd"), QStringLiteral("1")} + vbv;
    case EncoderFamily::Vaapi:
        return QStringList{QStringLiteral("-rc_mode"), QStringLiteral("CBR")} + vbv;
    case EncoderFamily::MediaFoundation:
        return QStringList{QStringLiteral("-rate_control"), QStringLiteral("cbr"),
                           QStringLiteral("-b:v"), kbpsArg(plan.videoKbps)};
    case EncoderFamily::VideoToolbox:
        return QStringList{QStringLiteral("-b:v"), kbpsArg(plan.videoKbps),
                           QStringLiteral("-maxrate"), kbpsArg(plan.maxRateKbps)};
    case EncoderFamily::OpenH264:
        return QStringList{QStringLiteral("-b:v"), kbpsArg(plan.videoKbps)};
    }
    return std::nullopt;
}

}  // namespace

std::optional<VideoEncoderConfig> configureVideoEncoder(
    const QString& codec,
    VideoRateControl control,
    const VideoRatePlan& plan)
{
    const std::optional<EncoderProfile> profile = encoderProfile(codec);
    if (!profile) {
        return std::nullopt;
    }
    std::optional<QStringList> rateArgs;
    switch (control) {
    case VideoRateControl::ConstantQuality:
        rateArgs = constantQualityArgs(profile->family, plan.quality);
        break;
    case VideoRateControl::AverageBitrate:
        rateArgs = averageBitrateArgs(profile->family, plan);
        break;
    case VideoRateControl::CappedBitrate:
        rateArgs = cappedBitrateArgs(profile->family, plan);
        break;
    }
    if (!rateArgs) {
        return std::nullopt;
    }
    VideoEncoderConfig config;
    config.codec = codec;
    config.isHardware = profile->hardware;
    config.needsVaapiHwUpload = profile->family == EncoderFamily::Vaapi;
    config.pixelFormat = pixelFormatFor(profile->family);
    config.rateControl = control;
    config.codecArgs = fixedArgs(*profile) + *rateArgs;
    return config;
}

}  // namespace miacode::video_export::detail
