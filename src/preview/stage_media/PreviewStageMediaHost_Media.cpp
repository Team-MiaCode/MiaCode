#include "preview/stage_media/PreviewStageMediaHost.h"

#include "core/chart/ChartAssetPaths.h"
#include "common/DebugLog.h"
#include "common/DebugOptions.h"
#include "common/FileContentStamp.h"
#include "common/OperationLog.h"
#include "core/video/PreviewVideoGeometryConfig.h"
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

QString normalizedLocalPath(const QString& path)
{
    return path.isEmpty() ? QString() : QDir::cleanPath(path);
}

}  // namespace

bool PreviewStageMediaHost::hasResolvedMedia() const
{
    return mediaKind_ != MediaKind::None;
}

bool PreviewStageMediaHost::hasVideoMedia() const
{
    return mediaKind_ == MediaKind::Video;
}

bool PreviewStageMediaHost::chartHasVideoBackground() const
{
    if (chartPath_.isEmpty()) {
        return false;
    }
    QString resolved = miacode::chart_assets::resolveChartVideoPath(
        chartPath_, chartVideoOverridePath_);
    if (resolved.isEmpty()) {
        resolved = resolveMediaPath(chartPath_);
    }
    return miacode::chart_assets::isVideoBackgroundPath(resolved);
}

bool PreviewStageMediaHost::hidePv() const
{
    return hidePv_;
}

void PreviewStageMediaHost::setHidePv(bool hide)
{
    if (hidePv_ == hide) {
        return;
    }
    hidePv_ = hide;
    emit hidePvChanged();
    reloadCurrentMediaPreservingPlayback();
}

void PreviewStageMediaHost::setMissingMediaFallbackEnabled(bool enabled)
{
    if (missingMediaFallbackEnabled_ == enabled) {
        return;
    }
    missingMediaFallbackEnabled_ = enabled;
    reloadCurrentMediaPreservingPlayback();
}

void PreviewStageMediaHost::reloadCurrentMediaPreservingPlayback()
{
    if (chartPath_.isEmpty()) {
        return;
    }
    const double restoreSecond = qMax(0.0, observedPlayheadSecond_);
    const bool restorePlaying = videoPlaybackActive_ || transportPlaying_;
    const QString chart = chartPath_;
    const QString override = chartVideoOverridePath_;
    chartPath_.clear();
    mediaStamp_.clear();
    setChartPath(chart, override);
    observedPlayheadSecond_ = restoreSecond;
    lastTimelineSecond_ = restoreSecond;
    if (mediaKind_ != MediaKind::Video) {
        return;
    }
    if (restorePlaying) {
        startPlayback(restoreSecond);
    } else {
        setPlayheadSeconds(restoreSecond);
    }
}


QString PreviewStageMediaHost::resolvedVideoPath() const
{
    if (mediaKind_ != MediaKind::Video) {
        return QString();
    }
    return mediaPath_;
}

int PreviewStageMediaHost::backgroundScaleMode() const
{
    return static_cast<int>(backgroundScaleMode_);
}

void PreviewStageMediaHost::setBackgroundScaleModeValue(int mode)
{
    if (mode == static_cast<int>(PreviewBackgroundScaleMode::SquareFitContain)) {
        setBackgroundScaleMode(PreviewBackgroundScaleMode::SquareFitContain);
        return;
    }
    if (mode == static_cast<int>(PreviewBackgroundScaleMode::InnerCircleFitOuterFill)) {
        setBackgroundScaleMode(PreviewBackgroundScaleMode::InnerCircleFitOuterFill);
        return;
    }
    setBackgroundScaleMode(
        mode == static_cast<int>(PreviewBackgroundScaleMode::FitContain)
            ? PreviewBackgroundScaleMode::FitContain
            : PreviewBackgroundScaleMode::FillCrop
    );
}

void PreviewStageMediaHost::setBackgroundScaleMode(PreviewBackgroundScaleMode mode)
{
    if (backgroundScaleMode_ == mode) {
        return;
    }
    const PreviewBackgroundScaleMode previousMode = backgroundScaleMode_;
    backgroundScaleMode_ = mode;
    // Report what refreshInnerVideoSinkForScaleMode() DID, not what the switch was
    // supposed to trigger. This is the only record tying a user-visible render-setting
    // change to the inner-sink side effect, which is what "switching into mode 3
    // mid-playback shows the wrong first frame" (primed=0) and "VRAM does not drop
    // after leaving mode 3" (cleared=0) each hinge on. The no-op case returns above
    // and stays unlogged.
    const InnerVideoSinkRefresh refresh = refreshInnerVideoSinkForScaleMode();
    appendPreviewStageMediaLog(
        QStringLiteral("scale_mode"),
        QStringLiteral("from=%1 to=%2 inner_sink_active=%3 primed=%4 cleared=%5")
            .arg(QString::fromLatin1(backgroundScaleModeToken(previousMode)))
            .arg(QString::fromLatin1(backgroundScaleModeToken(backgroundScaleMode_)))
            .arg(innerVideoSinkActive() ? 1 : 0)
            .arg(refresh == InnerVideoSinkRefresh::Primed ? 1 : 0)
            .arg(refresh == InnerVideoSinkRefresh::Cleared ? 1 : 0));
    emit backgroundScaleModeChanged();
}

bool PreviewStageMediaHost::innerVideoSinkActive() const
{
    return innerVideoSink_ != nullptr
        && backgroundScaleMode_ == PreviewBackgroundScaleMode::InnerCircleFitOuterFill;
}

PreviewStageMediaHost::InnerVideoSinkRefresh PreviewStageMediaHost::refreshInnerVideoSinkForScaleMode()
{
    if (innerVideoSink_ == nullptr || innerVideoSink_ == videoSink_) {
        return InnerVideoSinkRefresh::None;
    }
    if (innerVideoSinkActive()) {
        if (lastVideoFrame_.isValid()) {
            innerVideoSink_->setVideoFrame(lastVideoFrame_);
            return InnerVideoSinkRefresh::Primed;
        }
        // Entered the mode with nothing retained — the inner circle stays blank
        // until the next decoded frame lands.
        return InnerVideoSinkRefresh::None;
    }
    // Leaving the mode: drop the retained frame so the inner sink stops pinning
    // a decode-pool surface (same reason releaseVideoBackend() clears it).
    innerVideoSink_->setVideoFrame(QVideoFrame());
    return InnerVideoSinkRefresh::Cleared;
}

double PreviewStageMediaHost::layoutSquareScale() const
{
    return layoutSquareScale_;
}

void PreviewStageMediaHost::setLayoutSquareScale(double scale)
{
    const double normalized = miacode::preview_video::normalizedLayoutSquareScale(scale);
    if (qFuzzyCompare(layoutSquareScale_ + 1.0, normalized + 1.0)) {
        return;
    }
    layoutSquareScale_ = normalized;
    emit layoutSquareScaleChanged();
}

void PreviewStageMediaHost::setChartPath(const QString& chartPath,
                                         const QString& chartVideoOverridePath)
{
    const QString normalizedChartPath = normalizedLocalPath(chartPath);
    // Resolve the media up-front so the skip decision can be content-aware.
    // Keying on (chart-path, video-override) jointly is necessary but NOT
    // sufficient: the user may keep the same chart + same `&video=` yet rewrite
    // the bg/pv bytes in place (the in-app video tools do exactly that, and
    // users swap same-named files), so a same-named clip with new content must
    // still force a re-decode. Phase 4c — explicit `&video=` beats the sibling
    // heuristic; fall back to the legacy resolver for image-only backgrounds.
    QString resolvedPath;
    if (!normalizedChartPath.isEmpty()) {
        resolvedPath = resolveStageMediaPath(normalizedChartPath, chartVideoOverridePath);
    }
    const QString mediaStamp = miacode::fs::fileContentStamp(resolvedPath);
    if (normalizedChartPath == chartPath_
        && chartVideoOverridePath == chartVideoOverridePath_
        && mediaStamp == mediaStamp_) {
        return;
    }

    chartPath_ = normalizedChartPath;
    chartVideoOverridePath_ = chartVideoOverridePath;
    mediaStamp_ = mediaStamp;
    playbackRateLogGate_.reset();
    clearMedia();
    if (chartPath_.isEmpty()) {
        appendPreviewStageMediaLog(QStringLiteral("set_chart_path"), QStringLiteral("chart=(empty) kind=none"));
        return;
    }

    mediaPath_ = resolvedPath;
    if (mediaPath_.isEmpty()) {
        appendPreviewStageMediaLog(
            QStringLiteral("set_chart_path"),
            QStringLiteral("chart=%1 media=(none) kind=none").arg(chartPath_));
        return;
    }
    const bool isVideo = miacode::chart_assets::isVideoBackgroundPath(mediaPath_);
    if (isVideo) {
        loadVideoMedia(mediaPath_);
    } else {
        loadImageMedia(mediaPath_);
    }

    appendPreviewStageMediaLog(
        QStringLiteral("set_chart_path"),
        QStringLiteral("chart=%1 media=%2 kind=%3 override=%4")
            .arg(chartPath_)
            .arg(mediaPath_)
            .arg(debugMediaTypeName())
            .arg(chartVideoOverridePath_.isEmpty()
                    ? QStringLiteral("(none)")
                    : chartVideoOverridePath_)
    );
}

void PreviewStageMediaHost::clearMedia()
{
    clearPvMemorySource();
    if (videoFrameConnection_) {
        QObject::disconnect(videoFrameConnection_);
        videoFrameConnection_ = QMetaObject::Connection();
    }
    lastVideoFrame_ = QVideoFrame();
    lastFramePtsSeconds_ = -1.0;
    videoBackendLoaded_ = false;
    // Push an empty frame so the QML VideoOutput's sink RELEASES the last
    // decoded frame. A retained QtAVPlayer QVideoFrame transitively holds a
    // QAVStream -> QSharedPointer<QAVFormatContext>, which keeps the FFmpeg
    // avio handle on the video file OPEN even after the QAVPlayer is destroyed
    // (the QML VideoOutput outlives this host). That leaked read handle is the
    // real cause of "pv占用" / "无法替换原文件": read still works but rename is
    // refused. Dropping the sink frame lets the format-context refcount reach
    // zero so ~QAVFormatContext -> avformat_close_input runs. Must happen
    // BEFORE videoSink_.clear() (which only nulls our QPointer, not the sink's
    // frame). See project_pv_file_lock_release.
    if (videoSink_ != nullptr) {
        videoSink_->setVideoFrame(QVideoFrame());
    }
    videoSink_.clear();
    // handleDecodedVideoFrame pushes each decoded frame into BOTH the outer sink
    // and the inner-circle sink (InnerCircleFitOuterFill scale mode). A frame
    // left in the inner sink pins the same QAVFormatContext -> pv.mp4 stays open.
    // Release it symmetrically with the outer sink.
    if (innerVideoSink_ != nullptr) {
        innerVideoSink_->setVideoFrame(QVideoFrame());
    }
    innerVideoSink_.clear();
    if (player_ != nullptr) {
        player_->stop();
        player_->setSource(QString());
    }
    ++videoSourceGeneration_;
    mediaKind_ = MediaKind::None;
    // A paused seek can be in flight when the media route is cleared (e.g.
    // DocumentSessionHost::clearTimelineAndPreview() requests a paused seek
    // and then clears the chart path ~9 lines later). This host owns that
    // handshake, so abandoning it here must still TERMINATE it — the
    // decoder that would have settled it via a frame/position update/timeout
    // is going away with the rest of this reset. Without this ack,
    // PlaybackCoordinator's own pausedSeekMediaPending_ gate
    // (runtime/playback/Seek.cpp) never releases, since it is only cleared
    // from handlePausedPreviewMediaSeekCompleted(). Mirrors the existing
    // timeout fallback (schedulePausedSeekTimeout), which uses the same
    // signal to mean "stop waiting for this seek".
    const bool hadPendingPausedSeek = pausedSeekCompletionPending_;
    const double abandonedPausedSeekSecond = pausedSeekTargetSecond_;
    const quint64 abandonedPausedSeekGeneration = pausedSeekGeneration_;
    pausedSeekCompletionPending_ = false;
    pausedSeekTargetMs_ = -1;
    pausedSeekTargetSecond_ = 0.0;
    pausedSeekGeneration_ = 0;
    ++pausedSeekTimeoutSerial_;
    preparedPlaybackPending_ = false;
    preparedPlaybackReady_ = false;
    preparedPlaybackLandingConfirmed_ = false;
    preparedPlaybackTargetMs_ = -1;
    preparedPlaybackTargetSecond_ = 0.0;
    preparedPlaybackTransaction_ = 0;
    ++preparedPlaybackTimeoutSerial_;
    mediaPath_.clear();
    imageSource_ = QUrl();
    lastTimelineSecond_ = 0.0;
    lastSeekMs_ = -1;
    videoPlaybackActive_ = false;
    videoPlaybackPendingStart_ = false;
    observedPlayheadSecond_ = 0.0;
    clockDeltaSeconds_ = 0.0;
    resetVideoFrameDiagnostics();
    resetStaleEndOfMediaRecovery();
    if (shuttingDown_) {
        return;
    }
    if (hadPendingPausedSeek) {
        appendPreviewStageMediaLog(
            QStringLiteral("paused_seek_media_ack"),
            QString("generation=%1 second=%2 source=clear_media")
                .arg(abandonedPausedSeekGeneration)
                .arg(abandonedPausedSeekSecond, 0, 'f', 6));
        emit pausedSeekCompleted(abandonedPausedSeekSecond, abandonedPausedSeekGeneration);
    }
    emit imageSourceChanged();
    emit mediaStateChanged();
    emit diagnosticsChanged();
}

bool PreviewStageMediaHost::releaseDecoderForFileReplace()
{
    MC_OP("PreviewStageMediaHost::releaseDecoderForFileReplace");
    // Soft path first: drops the retained sink frames (both sinks), unloads the
    // demuxer, resets media state and generation counters. On its own this is
    // not enough on Windows because the QAVPlayer keeps its own
    // QSharedPointer<QAVFormatContext> ref alive until its async unload lands, so
    // the pv.mp4 avio handle can still be open right after this returns.
    recordPvMemoryBoundary(PvMemoryBoundary::PlayerDestroyBefore);
    clearMedia();
    destroyPvMemorySource();
    // Destroy the player so ~QAVPlayer runs synchronously: it stops and JOINS the
    // demux/decode threads and releases the format context, so
    // avformat_close_input has executed by the time we return. This is the same
    // hard teardown; initializeBackendObjects() rebuilds a fresh player on the next load.
    if (player_ != nullptr) {
        player_->stop();
        player_->setSource(QString());
        delete player_;
        player_ = nullptr;
    }
    videoBackendLoaded_ = false;
    appendPreviewStageMediaLog(
        QStringLiteral("release_decoder_for_file_replace"),
        QStringLiteral("player_destroyed=1"));
    return player_ == nullptr && !hasResolvedMedia() && mediaPath_.isEmpty();
}

QString PreviewStageMediaHost::resolveMediaPath(const QString& chartPath) const
{
    return miacode::chart_assets::resolveBackgroundMediaPath(chartPath);
}

QString PreviewStageMediaHost::resolveStageMediaPath(
    const QString& chartPath, const QString& chartVideoOverridePath) const
{
    QString resolved = miacode::chart_assets::resolveChartVideoPath(
        chartPath, chartVideoOverridePath);
    if (resolved.isEmpty()) {
        resolved = resolveMediaPath(chartPath);
    }
    if (resolved.isEmpty()) {
        return missingMediaFallbackEnabled_
            ? miacode::chart_assets::resolveDisplayBackgroundImagePath(chartPath)
            : QString();
    }
    if (!hidePv_ && miacode::chart_assets::isVideoBackgroundPath(resolved)) {
        return resolved;
    }
    const QString imagePath = miacode::chart_assets::resolveUsableBackgroundImagePath(chartPath);
    if (!imagePath.isEmpty() || !missingMediaFallbackEnabled_) {
        return imagePath;
    }
    return miacode::chart_assets::resolveDisplayBackgroundImagePath(chartPath);
}

void PreviewStageMediaHost::loadImageMedia(const QString& path)
{
    imageSource_ = miacode::chart_assets::displayBackgroundImageUrl(path);
    mediaKind_ = MediaKind::Image;
    ++videoSourceGeneration_;
    pausedSeekCompletionPending_ = false;
    pausedSeekTargetMs_ = -1;
    pausedSeekTargetSecond_ = 0.0;
    pausedSeekGeneration_ = 0;
    ++pausedSeekTimeoutSerial_;
    preparedPlaybackPending_ = false;
    preparedPlaybackReady_ = false;
    preparedPlaybackLandingConfirmed_ = false;
    preparedPlaybackTargetMs_ = -1;
    preparedPlaybackTargetSecond_ = 0.0;
    preparedPlaybackTransaction_ = 0;
    ++preparedPlaybackTimeoutSerial_;
    lastTimelineSecond_ = 0.0;
    lastSeekMs_ = -1;
    videoPlaybackActive_ = false;
    videoPlaybackPendingStart_ = false;
    resetVideoFrameDiagnostics();
    resetStaleEndOfMediaRecovery();
    updateClockDelta();
    emit imageSourceChanged();
    emit mediaStateChanged();
    emit diagnosticsChanged();
}

void PreviewStageMediaHost::loadVideoMedia(const QString& path)
{
    MC_OP("PreviewStageMediaHost::loadVideoMedia");
    initializeBackendObjects();
    if (player_ == nullptr) {
        return;
    }

    imageSource_ = QUrl();
    mediaKind_ = MediaKind::Video;
    softwareDecodeFallbackTried_ = false;
    videoBackendLoaded_ = false;
    lastVideoFrame_ = QVideoFrame();
    lastFramePtsSeconds_ = -1.0;
    pausedSeekCompletionPending_ = false;
    pausedSeekTargetMs_ = -1;
    pausedSeekTargetSecond_ = 0.0;
    pausedSeekGeneration_ = 0;
    ++pausedSeekTimeoutSerial_;
    preparedPlaybackPending_ = false;
    preparedPlaybackReady_ = false;
    preparedPlaybackLandingConfirmed_ = false;
    preparedPlaybackTargetMs_ = -1;
    preparedPlaybackTargetSecond_ = 0.0;
    preparedPlaybackTransaction_ = 0;
    ++preparedPlaybackTimeoutSerial_;
    lastTimelineSecond_ = 0.0;
    lastSeekMs_ = -1;
    videoPlaybackActive_ = false;
    videoPlaybackPendingStart_ = false;
    resetVideoFrameDiagnostics();
    resetStaleEndOfMediaRecovery();
    bindVideoOutput();
    beginPvMemorySource();

    // (Re)connect the decoded-frame stream, capturing the current source
    // generation by value (clearMedia() bumped it just before this). Any
    // in-flight frame from a previous source carries the old generation and is
    // dropped in handleDecodedVideoFrame. Default (auto/queued) connection ->
    // the handler runs on the GUI thread.
    if (videoFrameConnection_) {
        QObject::disconnect(videoFrameConnection_);
        videoFrameConnection_ = QMetaObject::Connection();
    }
    const quint64 sourceGeneration = videoSourceGeneration_;
    videoFrameConnection_ = connect(player_, &QAVPlayer::videoFrame, this,
        [this, sourceGeneration](const QAVVideoFrame& avFrame) {
            const double ptsSeconds = avFrame.pts();
            const double durationSeconds = avFrame.duration();
            // Implicit QAVVideoFrame -> QVideoFrame; a platform hardware frame
            // keeps its RHI texture handle through this conversion.
            QVideoFrame vf = avFrame;
            handleDecodedVideoFrame(vf, ptsSeconds, durationSeconds, sourceGeneration);
        });

    player_->setSource(path);
    player_->setSpeed(static_cast<qreal>(playbackRate_));
    // Queued before LoadedMedia by QtAVPlayer; lands the player paused on the
    // first frame so the bg shows immediately and playback is timeline-driven.
    player_->pause();
    player_->seek(0);
    updateClockDelta();
    emit imageSourceChanged();
    emit mediaStateChanged();
    emit diagnosticsChanged();
    return;
}

void PreviewStageMediaHost::bindVideoOutput()
{
    // Push model: resolve the QVideoSink owned by the QML VideoOutput; decoded
    // frames are pushed into it from handleDecodedVideoFrame. The decoded-frame
    // signal connection itself is owned by loadVideoMedia (per source
    // generation), so this only (re)binds the sink target.
    videoSink_.clear();
    innerVideoSink_.clear();
    const auto resolveSink = [](QObject* videoOutputObject) -> QVideoSink* {
        if (videoOutputObject == nullptr) {
            return nullptr;
        }
        QObject* sinkObject = nullptr;
        const QVariant sinkVariant = videoOutputObject->property("videoSink");
        if (sinkVariant.isValid()) {
            sinkObject = sinkVariant.value<QObject*>();
        }
        return qobject_cast<QVideoSink*>(sinkObject);
    };
    videoSink_ = resolveSink(videoOutputObject_);
    innerVideoSink_ = resolveSink(innerVideoOutputObject_);
    if (lastVideoFrame_.isValid()) {
        if (videoSink_ != nullptr) {
            videoSink_->setVideoFrame(lastVideoFrame_);
        }
        if (innerVideoSinkActive() && innerVideoSink_ != videoSink_) {
            innerVideoSink_->setVideoFrame(lastVideoFrame_);
        }
    }
    appendPreviewStageMediaLog(
        QStringLiteral("bind_video_output"),
        QStringLiteral("attached=%1 sink=%2 inner_attached=%3 inner_sink=%4")
            .arg(videoOutputObject_ != nullptr ? 1 : 0)
            .arg(videoSink_ != nullptr ? 1 : 0)
            .arg(innerVideoOutputObject_ != nullptr ? 1 : 0)
            .arg(innerVideoSink_ != nullptr ? 1 : 0));
    return;
}
