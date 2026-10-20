#include <QCoreApplication>
#include <QTextStream>

#include <optional>

#include "export/video_export/VideoExportRuntimePolicy.h"

namespace {

bool require(bool condition, const QString& message, QTextStream& err)
{
    if (!condition) {
        err << message << Qt::endl;
        return false;
    }
    return true;
}

bool verifyPolicy(QTextStream& err)
{
    using miacode::video_export::effectiveVideoExportAudioBitrateKbps;
    using miacode::video_export::effectiveVideoExportX264Crf;
    using miacode::video_export::VideoExportSizePreset;
    using miacode::video_export::shouldRequestOffscreenPboReadback;
    using miacode::video_export::shouldRetryVideoExportWorkerAfterCrash;
    using miacode::video_export::shouldUsePremultipliedExportPipe;
    using miacode::video_export::videoExportSizePolicy;
    using miacode::video_export::videoExportSizePresetToken;

    const auto standard = videoExportSizePolicy(VideoExportSizePreset::Standard);
    const auto compact = videoExportSizePolicy(VideoExportSizePreset::Compact);
    const auto ultraWithPv = videoExportSizePolicy(VideoExportSizePreset::UltraCompactWithPv);
    const auto ultra = videoExportSizePolicy(VideoExportSizePreset::UltraCompact);
    if (!require(standard.gopSeconds > 0 && compact.gopSeconds > 0
                     && ultra.gopSeconds > 0 && ultra.minBitrateKbps > 0
                     && compact.minBitrateKbps <= compact.maxBitrateKbps
                     && ultra.minBitrateKbps <= ultra.maxBitrateKbps
                     && ultraWithPv.minBitrateKbps <= ultraWithPv.maxBitrateKbps
                     && ultra.disableVideoBackground && !ultraWithPv.disableVideoBackground
                     && videoExportSizePresetToken(VideoExportSizePreset::UltraCompactWithPv)
                         != videoExportSizePresetToken(VideoExportSizePreset::UltraCompact),
                 QStringLiteral("size presets expose valid budgets and distinct PV behavior"), err)) {
        return false;
    }

    for (const auto preset : {VideoExportSizePreset::Standard, VideoExportSizePreset::Compact,
                              VideoExportSizePreset::UltraCompactWithPv, VideoExportSizePreset::UltraCompact}) {
        const auto policy = videoExportSizePolicy(preset);
        if (!require(effectiveVideoExportX264Crf(preset, 22) == 22 + policy.x264CrfAdjustment
                         && effectiveVideoExportAudioBitrateKbps(preset, 320)
                             == qMin(320, policy.maxAudioBitrateKbps),
                     QStringLiteral("encoder settings apply the selected size policy"), err)) return false;
    }

    if (!require(
            shouldRequestOffscreenPboReadback(std::nullopt, std::nullopt),
            QStringLiteral("default export PBO policy should be enabled when no env override is set"),
            err)) {
        return false;
    }
    if (!require(
            shouldRequestOffscreenPboReadback(true, std::nullopt),
            QStringLiteral("ENABLE=1 should keep PBO requested"),
            err)) {
        return false;
    }
    if (!require(
            !shouldRequestOffscreenPboReadback(false, std::nullopt),
            QStringLiteral("ENABLE=0 should disable PBO"),
            err)) {
        return false;
    }
    if (!require(
            !shouldRequestOffscreenPboReadback(true, true),
            QStringLiteral("DISABLE=1 should override ENABLE=1"),
            err)) {
        return false;
    }
    if (!require(
            shouldRetryVideoExportWorkerAfterCrash(true, true, false, 1),
            QStringLiteral("CrashExit with PBO enabled should retry once"),
            err)) {
        return false;
    }
    if (!require(
            !shouldRetryVideoExportWorkerAfterCrash(true, false, false, 1),
            QStringLiteral("CrashExit without PBO should not retry"),
            err)) {
        return false;
    }
    if (!require(
            !shouldRetryVideoExportWorkerAfterCrash(false, true, false, 1),
            QStringLiteral("NormalExit failure should not retry"),
            err)) {
        return false;
    }
    if (!require(
            !shouldRetryVideoExportWorkerAfterCrash(true, true, true, 1),
            QStringLiteral("Canceled crash should not retry"),
            err)) {
        return false;
    }
    if (!require(
            !shouldRetryVideoExportWorkerAfterCrash(true, true, false, 2),
            QStringLiteral("Crash retry should happen at most once"),
            err)) {
        return false;
    }
    if (!require(
            shouldUsePremultipliedExportPipe(true, true, std::nullopt),
            QStringLiteral("fast D3D11 export should default to premultiplied transport"),
            err)) {
        return false;
    }
    if (!require(
            !shouldUsePremultipliedExportPipe(true, false, std::nullopt),
            QStringLiteral("high-quality export should keep straight RGBA transport"),
            err)) {
        return false;
    }
    if (!require(
            !shouldUsePremultipliedExportPipe(false, true, std::nullopt),
            QStringLiteral("OpenGL export should keep straight RGBA transport"),
            err)) {
        return false;
    }
    if (!require(
            !shouldUsePremultipliedExportPipe(true, true, false),
            QStringLiteral("explicit premultiplied transport opt-out should win"),
            err)) {
        return false;
    }
    return true;
}

}  // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream err(stderr);
    QTextStream out(stdout);

    if (!verifyPolicy(err)) {
        return 1;
    }

    out << "video_export_runtime_policy_spec ok" << Qt::endl;
    return 0;
}
