#include "preview/stage_media/PreviewStageMediaHost.h"

#include "core/chart/ChartAssetPaths.h"
#include "common/DebugLog.h"
#include "common/DebugOptions.h"
#include "common/FileContentStamp.h"
#include "common/OperationLog.h"
#include "preview/stage_media/PreviewSharedD3D11Device.h"  // H2: single_device= log field

#include <cstdio>  // G2 Diag: std::snprintf for sync rate-change beacon lines

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dxgi.h>          // F1: DXGI adapter probe to auto-detect integrated GPUs
#include <wrl/client.h>
#pragma comment(lib, "dxgi.lib")
#endif

// Windows preview decode backend: FFmpeg via QtAVPlayer. QT_AVPLAYER_MULTIMEDIA
// turns on the QAVVideoFrame -> QVideoFrame bridge (the conversion we feed to
// the QML VideoOutput sink). Still need QVideoFrame/QVideoSink for delivery.
#ifndef QT_AVPLAYER_MULTIMEDIA
#define QT_AVPLAYER_MULTIMEDIA
#endif
#include <QtAVPlayer/qavplayer.h>
#include <QtAVPlayer/qavvideoframe.h>
#include <QVideoFrame>
#include <QVideoSink>
#if defined(Q_OS_WIN)
#include <QtAVPlayer/qavd3d11sharedcontext_p.h>  // HW-decode diag counters / seek catch-up
#endif

#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QTimer>
#include <QVariant>
#include <QtMath>

#include "preview/stage_media/PreviewStageMediaHostInternal.h"

using namespace miacode::preview::psmh_detail;

namespace {

// F1: detect whether the render GPU is integrated, so auto mode can default
// preview decode to software (D3D11VA is unreliable on iGPUs). Probes DXGI
// adapter 0 — the adapter driving the primary output, which Qt's D3D11 RHI uses
// by default. Integrated GPUs share system memory and report little/no
// DedicatedVideoMemory; discrete GPUs report >=1GB. One-time, cached. Probe
// failure or a discrete adapter => false (keep hardware). Caller logs the result.
bool detectIntegratedRenderAdapter(QString *descOut)
{
#ifdef Q_OS_WIN
    using Microsoft::WRL::ComPtr;
    ComPtr<IDXGIFactory1> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
        return false;
    ComPtr<IDXGIAdapter1> adapter;
    if (FAILED(factory->EnumAdapters1(0, &adapter)))
        return false;
    DXGI_ADAPTER_DESC1 desc{};
    if (FAILED(adapter->GetDesc1(&desc)))
        return false;
    if (descOut)
        *descOut = QString::fromWCharArray(desc.Description);
    if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        return false; // WARP / software renderer, not an iGPU
    const quint64 dedicatedMb =
        static_cast<quint64>(desc.DedicatedVideoMemory) / (1024ull * 1024ull);
    return dedicatedMb < 512; // < 512 MB dedicated VRAM => integrated
#else
    Q_UNUSED(descOut);
    return false;
#endif
}

bool isIntegratedRenderAdapter(QString *descOut = nullptr)
{
    static QString cachedDesc;
    static const bool cached = detectIntegratedRenderAdapter(&cachedDesc);
    if (descOut)
        *descOut = cachedDesc;
    return cached;
}

QString avMediaStatusName(QAVPlayer::MediaStatus status)
{
    switch (status) {
    case QAVPlayer::NoMedia:
        return QStringLiteral("NoMedia");
    case QAVPlayer::LoadedMedia:
        return QStringLiteral("LoadedMedia");
    case QAVPlayer::EndOfMedia:
        return QStringLiteral("EndOfMedia");
    case QAVPlayer::InvalidMedia:
        return QStringLiteral("InvalidMedia");
    default:
        return QStringLiteral("UnknownMediaStatus");
    }
}

QString avStateName(QAVPlayer::State state)
{
    switch (state) {
    case QAVPlayer::StoppedState:
        return QStringLiteral("StoppedState");
    case QAVPlayer::PlayingState:
        return QStringLiteral("PlayingState");
    case QAVPlayer::PausedState:
        return QStringLiteral("PausedState");
    default:
        return QStringLiteral("UnknownState");
    }
}


}  // namespace

void PreviewStageMediaHost::initializeBackendObjects()
{
    if (player_ != nullptr) {
        return;
    }

    // QAVVideoFrame must be a registered metatype for the queued videoFrame
    // delivery below (QtAVPlayer decode thread -> host GUI thread).
    qRegisterMetaType<QAVVideoFrame>();

    player_ = new QAVPlayer(this);
    // Windows installs D3D11 diagnostics here; other platforms use the
    // platform-neutral diagnostic hook.
    miacode::preview::installPreviewDecodeDiagnostics();
    player_->setSpeed(static_cast<qreal>(playbackRate_));
    // Decode mode: the persisted user preference (硬件渲染 / 软件渲染, default
    // HARDWARE) decides. MIACODE_PREVIEW_FORCE_SOFTWARE_VIDEO stays a dev override on
    // top (ForceSoftware / ForceHardware win; Auto / unset honors the user
    // preference). The session-only fallback latch forces FFmpeg CPU decode
    // only after the selected platform hardware decoder reports InvalidMedia.
    using DecodePref = miacode::debug_options::PreviewVideoDecodePreference;
    const DecodePref decodePref = miacode::debug_options::previewVideoDecodePreference();
    const bool useSoftware = videoDecodeUsesSoftware();
    if (useSoftware) {
        player_->setInputVideoCodec(QStringLiteral("software"));
    }
    const char *prefName = decodePref == DecodePref::ForceSoftware ? "force_sw"
                         : decodePref == DecodePref::ForceHardware ? "force_hw"
                                                                   : "auto";
#if defined(Q_OS_WIN)
    QString adapterDesc;
    const bool integratedGpu = isIntegratedRenderAdapter(&adapterDesc);
    // `probe_adapter` is a DXGI heuristic only; the Quick RHI device is logged
    // independently by quick_shell/device.
    appendPreviewStageMediaLog(
        QStringLiteral("media_backend"),
        QString("backend=qtavplayer ffmpeg=1 hardware_decoder=d3d11va qt_runtime_version=%1 force_software=%2 pref=%3 igpu=%4 probe_adapter=\"%5\" probe_adapter_source=dxgi_enum0_heuristic single_device=%6")
            .arg(QString::fromLatin1(qVersion()))
            .arg(useSoftware ? 1 : 0)
            .arg(QString::fromLatin1(prefName))
            .arg(integratedGpu ? 1 : 0)
            .arg(adapterDesc)
            .arg(miacode::preview::sharedPreviewD3D11DeviceActive() ? 1 : 0));
#elif defined(Q_OS_MACOS)
    appendPreviewStageMediaLog(
        QStringLiteral("media_backend"),
        QString("backend=qtavplayer ffmpeg=1 hardware_decoder=videotoolbox qt_runtime_version=%1 force_software=%2 pref=%3 renderer_bridge=metal")
            .arg(QString::fromLatin1(qVersion()))
            .arg(useSoftware ? 1 : 0)
            .arg(QString::fromLatin1(prefName)));
#elif defined(Q_OS_LINUX)
    appendPreviewStageMediaLog(
        QStringLiteral("media_backend"),
        QString("backend=qtavplayer ffmpeg=1 hardware_decoder=vaapi qt_runtime_version=%1 force_software=%2 pref=%3 renderer_bridge=drm_egl")
            .arg(QString::fromLatin1(qVersion()))
            .arg(useSoftware ? 1 : 0)
            .arg(QString::fromLatin1(prefName)));
#else
    appendPreviewStageMediaLog(
        QStringLiteral("media_backend"),
        QString("backend=qtavplayer ffmpeg=1 hardware_decoder=platform_default qt_runtime_version=%1 force_software=%2 pref=%3")
            .arg(QString::fromLatin1(qVersion()))
            .arg(useSoftware ? 1 : 0)
            .arg(QString::fromLatin1(prefName)));
#endif

    // 定位通知用于播放准备与末尾恢复；暂停 seek 等待视频帧送入显示端。
    seekedConnection_ = connect(player_, &QAVPlayer::seeked, this, [this](qint64 posMs) {
        if (mediaKind_ != MediaKind::Video) {
            return;
        }
        const double mediaSecond = static_cast<double>(posMs) / 1000.0;
        lastTimelineSecond_ = qMax(0.0, mediaSecond - timelineOffsetSeconds_);
        settlePendingSeekAcks(mediaSecond, mediaSecond, false);
        if (staleEndOfMediaResumePending_) {
            // The stale-EndOfMedia recovery seek landed, so the player's end-of-file
            // latch is cleared and play() will resume here instead of restarting the
            // clip from zero. See tryRecoverFromStaleEndOfMedia.
            staleEndOfMediaResumePending_ = false;
            ++staleEndOfMediaResumeSerial_;
            if (player_ != nullptr) {
                player_->play();
            }
            videoPlaybackActive_ = true;
            videoPlaybackPendingStart_ = false;
            videoPlaybackActiveElapsed_.restart();
            if (videoFrameElapsed_.isValid()) {
                videoFrameElapsed_.restart();
            }
            appendPreviewStageMediaLog(
                QStringLiteral("stale_end_of_media_resumed"),
                QString("second=%1 position_ms=%2 recoveries=%3")
                    .arg(mediaSecond, 0, 'f', 6)
                    .arg(posMs)
                    .arg(staleEndOfMediaRecoveries_));
            emit diagnosticsChanged();
        }
        updateClockDelta();
#if defined(Q_OS_WIN)
        // HW-decode diag: the demuxer seek landed; the decoder now catches up to the
        // target GOP (skipFrame decode-but-don't-display bursts). Arm the bounded
        // readback so seek-time green/garble is captured, zero the catch-up counter,
        // and start the latency clock — read back at the first DISPLAYED frame below.
        qavArmPreviewHwFrameDump();
        qavTakePreviewCatchupSkipCount();  // discard pre-seek residual
        seekCatchupTimer_.restart();
        seekLatencyPending_ = true;
        emitHwDecodeDiagSummary("seek");
#endif
    });

    connect(player_, &QAVPlayer::mediaStatusChanged, this, [this](QAVPlayer::MediaStatus status) {
        appendPreviewStageMediaLog(
            QStringLiteral("media_status"),
            QString("status=%1 state=%2 position_ms=%3 kind=%4 txn=%5 last_seek_ms=%6 "
                    "active_elapsed_ms=%7 frames=%8 last_pts_ms=%9 prepared_pending=%10")
                .arg(avMediaStatusName(status))
                .arg(avStateName(player_ != nullptr ? player_->state() : QAVPlayer::StoppedState))
                .arg(player_ != nullptr ? player_->position() : -1)
                .arg(debugMediaTypeName())
                .arg(playbackTransactionId_)
                .arg(lastSeekMs_)
                .arg(videoPlaybackActiveElapsed_.isValid() ? videoPlaybackActiveElapsed_.elapsed() : -1)
                .arg(videoFrameCountTotal_)
                .arg(lastFramePtsSeconds_ >= 0.0 ? qRound64(lastFramePtsSeconds_ * 1000.0) : -1)
                .arg(preparedPlaybackPending_ ? 1 : 0));
        if (status == QAVPlayer::LoadedMedia) {
            videoBackendLoaded_ = true;
            return;
        }
        if (status == QAVPlayer::NoMedia) {
            latePvMemoryNoMedia();
            return;
        }
        if (status == QAVPlayer::EndOfMedia) {
            // A PV/BG video is subordinate visual media, not the preview
            // transport's lifetime owner. Leave lastVideoFrame_ in both video
            // sinks so the final decoded frame stays visible while the chart,
            // BGM and SFX continue to the unified content-duration endpoint.
            const bool wasPlaybackActive = videoPlaybackActive_;
            videoPlaybackActive_ = false;
            videoPlaybackPendingStart_ = false;
            videoPlaybackActiveElapsed_.invalidate();
            recordPvMemoryBoundary(PvMemoryBoundary::EndOfMedia);
            updateClockDelta();
            updateVideoFrameStallState(true);
            emitHwDecodeDiagSummary("eom");
            handleVideoEndOfMedia(wasPlaybackActive);
            emit diagnosticsChanged();
            return;
        }
        if (status == QAVPlayer::InvalidMedia && mediaKind_ == MediaKind::Video) {
            maybeRetryWithSoftwareDecode();
        }
    });

    connect(player_, &QAVPlayer::errorOccurred, this, [this](QAVPlayer::Error error, const QString& errorString) {
        appendPreviewStageMediaLog(
            QStringLiteral("media_error"),
            QString("error=%1 text=\"%2\" kind=%3")
                .arg(static_cast<int>(error))
                .arg(errorString)
                .arg(debugMediaTypeName()));
    });
    return;
}


void PreviewStageMediaHost::setPlaybackRate(double rate)
{
    MC_OP("PreviewStageMediaHost::setPlaybackRate");
    playbackRate_ = qMax(0.05, rate);
    // The whole reason for the migration: QAVPlayer::setSpeed applies the rate
    // inside QtAVPlayer's own decode loop. It does NOT rebuild a Qt converter
    // pipeline mid-flight, so it cannot race the QSG D3D11 texture
    // sampler — i.e. the "倍速闪退" crash class is structurally gone, and all
    // the deferred-apply / recover-rebuild scaffolding below is unnecessary.
    if (player_ != nullptr) {
        player_->setSpeed(static_cast<qreal>(playbackRate_));
    }
    const miacode::diagnostics::StageRateKey rateKey{
        playbackRate_, static_cast<int>(mediaKind_)};
    if (playbackRateLogGate_.shouldEmit(
            miacode::diagnostics::PlaybackRateLogKind::Ordinary, rateKey)) {
        appendPreviewStageMediaLog(
            QStringLiteral("playback_rate"),
            QString("rate=%1 kind=%2")
                .arg(playbackRate_, 0, 'f', 3)
                .arg(debugMediaTypeName()));
    }
}


void PreviewStageMediaHost::maybeRetryWithSoftwareDecode()
{
    if (softwareDecodeFallbackTried_
        || player_ == nullptr
        || mediaKind_ != MediaKind::Video
        || mediaPath_.isEmpty()) {
        appendPreviewStageMediaLog(
            QStringLiteral("video_software_fallback_skip"),
            QString("tried=%1 has_player=%2 kind=%3 has_path=%4")
                .arg(softwareDecodeFallbackTried_ ? 1 : 0)
                .arg(player_ != nullptr ? 1 : 0)
                .arg(debugMediaTypeName())
                .arg(mediaPath_.isEmpty() ? 0 : 1));
        return;
    }
    softwareDecodeFallbackTried_ = true;
    appendPreviewStageMediaLog(
        QStringLiteral("video_software_fallback"),
        QString("path=%1 reason=invalid_media resume_ms=%2 resume_playing=%3")
            .arg(mediaPath_)
            .arg(lastSeekMs_ >= 0 ? lastSeekMs_ : 0)
            .arg(videoPlaybackActive_ ? 1 : 0));
    const qint64 resumeMs = lastSeekMs_ >= 0 ? lastSeekMs_ : 0;
    const bool resumePlaying = videoPlaybackActive_;
    // Re-open forcing FFmpeg software decode (same as QT_AVPLAYER_NO_HWDEVICE).
    // setInputVideoCodec must precede setSource to apply on (re)load; the empty
    // setSource forces a reload since setSource(sameUrl) is a no-op.
    player_->stop();
    player_->setInputVideoCodec(QStringLiteral("software"));
    player_->setSource(QString());
    player_->setSource(mediaPath_);
    player_->setSpeed(static_cast<qreal>(playbackRate_));
    player_->seek(resumeMs);
    if (resumePlaying) {
        player_->play();
    } else {
        player_->pause();
    }
}

void PreviewStageMediaHost::reloadVideoDecodeInPlace()
{
    if (player_ == nullptr || mediaKind_ != MediaKind::Video || mediaPath_.isEmpty()) {
        return;
    }
    // Capture the live position + play state, flip the decoder on the SAME player,
    // reload in place (the empty setSource forces a reload since setSource(sameUrl)
    // is a no-op), then restore position + play state. Reuses the existing video
    // sink — no player recreation, no app restart. Bidirectional vs the one-way
    // software fallback: empty codec restores the platform hardware decoder;
    // "software" selects FFmpeg CPU decode.
    const double second = qMax(0.0, currentPlaybackSecond());
    const qint64 resumeMs = qMax<qint64>(0, qRound64((second + timelineOffsetSeconds_) * 1000.0));
    const bool resumePlaying = videoPlaybackActive_;
    appendPreviewStageMediaLog(
        QStringLiteral("video_decode_reload"),
        QString("prefer_software=%1 resume_ms=%2 resume_playing=%3 path=%4")
            .arg(videoDecodePreferSoftware_ ? 1 : 0)
            .arg(resumeMs)
            .arg(resumePlaying ? 1 : 0)
            .arg(mediaPath_));
    player_->stop();
    player_->setInputVideoCodec(
        videoDecodeUsesSoftware() ? QStringLiteral("software") : QString());
    player_->setSource(QString());
    player_->setSource(mediaPath_);
    player_->setSpeed(static_cast<qreal>(playbackRate_));
    player_->seek(resumeMs);
    if (resumePlaying) {
        player_->play();
    } else {
        player_->pause();
    }
}

bool PreviewStageMediaHost::videoDecodeUsesSoftware() const
{
    using DecodePref = miacode::debug_options::PreviewVideoDecodePreference;
    const DecodePref decodePref = miacode::debug_options::previewVideoDecodePreference();
    bool forceSoftware = videoDecodePreferSoftware_;
    switch (decodePref) {
    case DecodePref::ForceSoftware: forceSoftware = true; break;
    case DecodePref::ForceHardware: forceSoftware = false; break;
    case DecodePref::Auto:          break;
    }
    return softwareDecodeFallbackTried_ || forceSoftware;
}

QString PreviewStageMediaHost::videoDecodeDescription() const
{
    if (mediaKind_ != MediaKind::Video || !hasVideoMedia()) {
        return QStringLiteral("None");
    }
    if (videoDecodeUsesSoftware()) {
        return QStringLiteral("CPU (Software)");
    }
    if (lastVideoFrame_.isValid()) {
        if (lastVideoFrame_.handleType() == QVideoFrame::RhiTextureHandle) {
#if defined(Q_OS_LINUX)
            return QStringLiteral("GPU (VA-API)");
#elif defined(Q_OS_WIN)
            return QStringLiteral("GPU (D3D11VA)");
#elif defined(Q_OS_MACOS)
            return QStringLiteral("GPU (VideoToolbox)");
#else
            return QStringLiteral("GPU (Hardware)");
#endif
        }
        return QStringLiteral("CPU (Software)");
    }
#if defined(Q_OS_LINUX)
    return QStringLiteral("GPU (VA-API)");
#elif defined(Q_OS_WIN)
    return QStringLiteral("GPU (D3D11VA)");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("GPU (VideoToolbox)");
#else
    return QStringLiteral("GPU (Hardware)");
#endif
}

void PreviewStageMediaHost::setVideoDecodePreference(bool preferSoftware)
{
    if (videoDecodePreferSoftware_ == preferSoftware) {
        return;
    }
    videoDecodePreferSoftware_ = preferSoftware;
    appendPreviewStageMediaLog(
        QStringLiteral("video_decode_preference"),
        QString("prefer_software=%1 has_player=%2 kind=%3")
            .arg(preferSoftware ? 1 : 0)
            .arg(player_ != nullptr ? 1 : 0)
            .arg(debugMediaTypeName()));
    // An explicit user choice clears the session-only auto-fallback latch so a
    // future backend rebuild honors the chosen mode (and so switching back to
    // hardware isn't immediately re-forced to software by a stale fallback flag).
    softwareDecodeFallbackTried_ = false;
    if (player_ != nullptr) {
        if (mediaKind_ == MediaKind::Video && !mediaPath_.isEmpty()) {
            reloadVideoDecodeInPlace();  // hot-switch the currently-loaded PV
        } else {
            // No PV loaded yet: apply now so the next load uses the chosen decoder.
            player_->setInputVideoCodec(
                videoDecodeUsesSoftware() ? QStringLiteral("software") : QString());
        }
    }
}
