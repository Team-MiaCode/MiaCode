#include "app/runtime/preview/StageMediaHost.h"
#include "app/runtime/Shared.h"
#include "app/runtime/playback/PlaybackCoordinator.h"
#include "app/runtime/shell/ShellHost.h"

#include "audio/QtPreviewSfxRuntime.h"
#include "core/chart/parser/SimaiParser.h"
#include "app/quick_shell/QuickShellPreviewCompositeSurface.h"
#include "app/quick_shell/QuickShellPreviewSurfacePolicy.h"
#include "core/chart/ChartAssetPaths.h"
#include "common/DebugLog.h"
#include "common/OperationLog.h"
#include "preview/runtime/PreviewRuntime.h"
#include "preview/stage_media/PreviewStageMediaHost.h"
#include "app/services/PreferenceDocument.h"
#include "core/scene/PreviewProgressStatsCache.h"
#include "core/chart/transform/ChartBatchTransform.h"
#include "core/chart/transform/ChartNormalization.h"
#include "core/analysis/MuriAnalyzer.h"
#include "core/analysis/MuriPanelEntries.h"
#include "core/analysis/MuriStaticChecker.h"

#include <QtCore>
#include <QtGui>

#include <cstdio>

using namespace miacode::runtime::shared;

miacode::runtime::StageMediaHost::StageMediaHost(
    Session& session,
    RuntimeContext::Ui& ui,
    RuntimeContext::State& state)
    : session_(session)
    , ui_(ui)
    , state_(state)
    , previewAppearanceValues_(session.previewAppearanceValues_)
{}

void miacode::runtime::StageMediaHost::applyPreviewStageMediaRouteVisualSettings()
{
    miacode::runtime::shared::applyPreviewStageMediaRouteVisualSettings(state_);
}

Session::PreviewStageMediaRoute miacode::runtime::StageMediaHost::previewStageMediaRoute() const
{
    return Session::PreviewStageMediaRoute::QuickShellStageHost;
}

bool miacode::runtime::StageMediaHost::previewUsesStageMediaHostRoute() const
{
    return true;
}

bool miacode::runtime::StageMediaHost::quickShellPreviewUsesSeparateSurface() const
{
    return false;
}

QWindow* miacode::runtime::StageMediaHost::quickShellPreviewCompositeWindow() const
{
    return state_.quickShellPreviewCompositeSurface_ != nullptr
        ? state_.quickShellPreviewCompositeSurface_->hostWindow()
        : nullptr;
}

bool miacode::runtime::StageMediaHost::shouldDeferQuickShellStartupStageMediaLoad() const
{
    return previewUsesStageMediaHostRoute() && state_.quickShellStartupStageMediaLoadDeferred_;
}

void miacode::runtime::StageMediaHost::noteQuickShellStartupUiReady()
{
    if (!previewUsesStageMediaHostRoute()) {
        return;
    }
    state_.quickShellStartupUiReady_ = true;
    scheduleDeferredQuickShellStartupStageMediaLoadIfReady();
}

void miacode::runtime::StageMediaHost::scheduleDeferredQuickShellStartupStageMediaLoadIfReady()
{
    if (!shouldDeferQuickShellStartupStageMediaLoad()
        || !state_.quickShellStartupUiReady_
        || state_.startupRestorePending_
        || state_.deferredQuickShellStartupStageMediaFlushScheduled_) {
        return;
    }

    state_.deferredQuickShellStartupStageMediaFlushScheduled_ = true;
    QTimer::singleShot(0, &session_, [this]() {
        state_.deferredQuickShellStartupStageMediaFlushScheduled_ = false;
        if (!shouldDeferQuickShellStartupStageMediaLoad()
            || !state_.quickShellStartupUiReady_
            || state_.startupRestorePending_) {
            return;
        }

        state_.quickShellStartupStageMediaLoadDeferred_ = false;
        if (!state_.deferredQuickShellStartupStageMediaPending_ || state_.previewStageMediaHost_ == nullptr) {
            return;
        }

        state_.deferredQuickShellStartupStageMediaPending_ = false;
        // Phase 4c — use the cached override threaded through the
        // deferred-load path (set in syncPreviewStageMediaRouteChartPath).
        state_.previewStageMediaHost_->setChartPath(
            state_.deferredQuickShellStartupStageMediaChartPath_,
            state_.deferredQuickShellStartupStageMediaVideoOverride_);
        state_.previewStageMediaHost_->setPlayheadSeconds(state_.deferredQuickShellStartupStageMediaPausedSecond_);
        refreshPreviewStageMediaRouteDebugState(false);
    });
}

void miacode::runtime::StageMediaHost::updatePreviewStageMediaPresentationMode(bool requestUpdate)
{
    if (state_.scene_ == nullptr) {
        return;
    }
    state_.scene_->setStageMediaPresentationMode(
        miacode::preview::scene::PreviewStageMediaPresentationMode::ExternalQuickMediaItem,
        requestUpdate
    );
}

void miacode::runtime::StageMediaHost::ensurePreviewStageMediaRouteInitialized()
{
    ensurePreviewStageMediaHostInitialized();
}

void miacode::runtime::StageMediaHost::syncPreviewStageMediaRouteChartPath(
    const QString& chartPath,
    const QString& trackPath,
    double pausedSecond,
    const QString& chartVideoOverridePath)
{
    Q_UNUSED(trackPath);
    const double clampedPausedSecond = qMax(0.0, pausedSecond);
    updatePreviewStageMediaPresentationMode(false);

    ensurePreviewStageMediaHostInitialized();
    state_.deferredQuickShellStartupStageMediaChartPath_ = chartPath;
    state_.deferredQuickShellStartupStageMediaPausedSecond_ = clampedPausedSecond;
    state_.deferredQuickShellStartupStageMediaPending_ = true;
    // Phase 4c — cache the override on the section so the
    // deferred-load completion in onPreviewStageMediaDeferredLoadReady
    // (around line 113) can apply it when the actual setChartPath fires.
    state_.deferredQuickShellStartupStageMediaVideoOverride_ = chartVideoOverridePath;
    if (!shouldDeferQuickShellStartupStageMediaLoad()) {
        state_.deferredQuickShellStartupStageMediaPending_ = false;
        state_.previewStageMediaHost_->setChartPath(chartPath, chartVideoOverridePath);
        state_.previewStageMediaHost_->setPlayheadSeconds(clampedPausedSecond);
    }

    applyPreviewStageMediaRoutePlaybackRate(state_.previewPlaybackRate_, "sync_chart_path");
    refreshPreviewStageMediaRouteDebugState(false);
}

void miacode::runtime::StageMediaHost::clearPreviewStageMediaRoute()
{
    state_.deferredQuickShellStartupStageMediaPending_ = false;
    state_.deferredQuickShellStartupStageMediaChartPath_.clear();
    state_.deferredQuickShellStartupStageMediaPausedSecond_ = 0.0;
    if (state_.previewStageMediaHost_ != nullptr) {
        state_.previewStageMediaHost_->setChartPath(QString());
    }

    refreshPreviewStageMediaRouteDebugState(false);
}

void miacode::runtime::StageMediaHost::releasePreviewStageMediaDecoderForFileOperation()
{
    if (state_.previewStageMediaHost_ != nullptr) {
        state_.previewStageMediaHost_->releaseDecoderForFileReplace();
    }
}

void miacode::runtime::StageMediaHost::applyPreviewMediaWarmupToStageMediaRoute(
    const QString& chartPath)
{
    ensurePreviewStageMediaHostInitialized();
    state_.deferredQuickShellStartupStageMediaChartPath_ = chartPath;
    state_.deferredQuickShellStartupStageMediaPending_ = true;
    if (!shouldDeferQuickShellStartupStageMediaLoad()) {
        state_.deferredQuickShellStartupStageMediaPending_ = false;
        state_.previewStageMediaHost_->setChartPath(chartPath);
    }

    applyPreviewStageMediaRoutePlaybackRate(state_.previewPlaybackRate_, "media_warmup_result");
    refreshPreviewStageMediaRouteDebugState(false);
}

void miacode::runtime::StageMediaHost::resetPreviewStageMediaRouteTimelineOffset()
{
    miacode::runtime::shared::resetPreviewStageMediaRouteTimelineOffset(state_);
}

void miacode::runtime::StageMediaHost::applyPreviewStageMediaRoutePlaybackRate(double rate, const char* site)
{
    miacode::runtime::shared::applyPreviewStageMediaRoutePlaybackRate(state_, rate, site);
}

bool miacode::runtime::StageMediaHost::previewStageMediaRouteHasVideo() const
{
    return miacode::runtime::shared::previewStageMediaRouteHasVideo(state_);
}

double miacode::runtime::StageMediaHost::previewStageMediaRouteCurrentPlaybackSecond() const
{
    return state_.previewStageMediaHost_ != nullptr ? state_.previewStageMediaHost_->currentPlaybackSecond() : 0.0;
}

void miacode::runtime::StageMediaHost::startPreviewStageMediaRoutePlayback(double second)
{
    if (state_.previewStageMediaHost_ != nullptr) {
        state_.previewStageMediaHost_->startPlayback(second);
    }
}

void miacode::runtime::StageMediaHost::syncPreviewStageMediaRoutePlayback(double second)
{
    miacode::runtime::shared::syncPreviewStageMediaRoutePlayback(state_, second);
}

void miacode::runtime::StageMediaHost::pausePreviewStageMediaRoutePlayback()
{
    miacode::runtime::shared::pausePreviewStageMediaRoutePlayback(state_);
}

void miacode::runtime::StageMediaHost::seekPreviewStageMediaRouteWhilePaused(double second)
{
    miacode::runtime::shared::seekPreviewStageMediaRouteWhilePaused(state_, qBound(0.0, second, session_.previewDurationSeconds()));
}

void miacode::runtime::StageMediaHost::submitPreviewStageMediaRoutePausedSeek(double second, quint64 generation)
{
    miacode::runtime::shared::submitPreviewStageMediaRoutePausedSeek(state_, qBound(0.0, second, session_.previewDurationSeconds()), generation);
}

void miacode::runtime::StageMediaHost::setPreviewStageMediaRouteObservedPlayheadSecond(double second)
{
    miacode::runtime::shared::setPreviewStageMediaRouteObservedPlayheadSecond(state_, second);
}

void miacode::runtime::StageMediaHost::ensureQuickShellPreviewCompositeSurfaceInitialized()
{
    if (state_.quickShellPreviewCompositeSurface_ == nullptr) {
        state_.quickShellPreviewCompositeSurface_ = new QuickShellPreviewCompositeSurface(&session_);
    }
    state_.quickShellPreviewCompositeSurface_->setRuntime(state_.scene_);
    state_.quickShellPreviewCompositeSurface_->setMediaHost(state_.previewStageMediaHost_);
}

void miacode::runtime::StageMediaHost::refreshQuickShellPreviewCompositeSurfaceState()
{
    miacode::runtime::shared::refreshQuickShellPreviewCompositeSurfaceState(state_, session_);
}

void miacode::runtime::StageMediaHost::ensurePreviewStageMediaHostInitialized()
{
    if (state_.previewStageMediaHost_ != nullptr) {
        ensureQuickShellPreviewCompositeSurfaceInitialized();
        refreshQuickShellPreviewCompositeSurfaceState();
        return;
    }

    state_.previewStageMediaHost_ = new PreviewStageMediaHost(&session_);
    state_.previewStageMediaHost_->setMissingMediaFallbackEnabled(session_.exportPreviewActive_);
    state_.previewStageMediaHost_->setBackgroundScaleMode(state_.previewBackgroundScaleMode_);
    state_.previewStageMediaHost_->setLayoutSquareScale(state_.previewLayoutSquareScale_);
    session_.playback_->setPreviewStageMediaFrameRateMode(state_.previewStageMediaFrameRateMode_, false);
    // Push the persisted video decode-mode preference (硬件渲染 / 软件渲染) once,
    // before any PV is resolved, so the user's choice applies on the first load.
    // Done directly (not via session_.setVideoDecodePrefersSoftware) because that
    // setter early-returns when the value is unchanged, which would skip the
    // initial host hand-off when the cached value equals the default (false).
    state_.previewStageMediaHost_->setVideoDecodePreference(session_.currentVideoDecodePrefersSoftware());
    {
        const QJsonObject ui = PreferenceDocument::loadPreferencesObject()
                                   .value(QLatin1String("ui")).toObject();
        state_.previewStageMediaHost_->setHidePv(
            ui.value(QLatin1String("preview_hide_pv")).toBool(false));
    }
    QObject::connect(state_.previewStageMediaHost_, &PreviewStageMediaHost::mediaStateChanged, &session_, [this]() {
        applyPreviewStageMediaRouteVisualSettings();
        refreshQuickShellPreviewCompositeSurfaceState();
        refreshPreviewStageMediaRouteDebugState(false);
    });
    QObject::connect(state_.previewStageMediaHost_, &PreviewStageMediaHost::playbackPositionChanged, &session_, [this](double second) {
        if (state_.playing_) {
            return;
        }
        if (state_.pausedSeekMediaPending_) {
            if (state_.runtimeDebugOutputEnabled_) {
                miacode::debug_log::appendLine(
                    miacode::debug_log::Channel::Audio,
                    QStringLiteral("preview/stage_media"),
                    QString("action=paused_seek_media_drop second=%1 reason=pending_generation")
                        .arg(second, 0, 'f', 6));
            }
            refreshPreviewStageMediaRouteDebugState(false);
            return;
        }
        // Non-command write: the media backend is reporting where it actually
        // paused, not answering a seek — see PlaybackStateAuthority.h. The
        // guard above (playing_ / pausedSeekMediaPending_) is duplicated
        // inside reanchorObservedSecond itself, so the write stays safe for
        // any future caller of the port; this call site keeps its own copy
        // only to gate the diagnostics above.
        if (auto* authority = session_.applicationServices_.playbackStateAuthority(); authority != nullptr) {
            authority->reanchorObservedSecond(second);
        }
        refreshPreviewStageMediaRouteDebugState(false);
    });
    QObject::connect(
        state_.previewStageMediaHost_,
        &PreviewStageMediaHost::playbackStartPrepared,
        &session_,
        [this](double second, quint64 transactionId) {
            session_.playback_->handlePreviewStartupVideoPrepared(second, transactionId);
        }
    );
    QObject::connect(
        state_.previewStageMediaHost_,
        &PreviewStageMediaHost::pausedSeekCompleted,
        &session_,
        [this](double second, quint64 generation) {
            session_.handlePausedPreviewMediaSeekCompleted(second, generation);
        }
    );
    QObject::connect(state_.previewStageMediaHost_, &PreviewStageMediaHost::diagnosticsChanged, &session_, [this]() {
        refreshPreviewStageMediaRouteDebugState(!state_.playing_);
    });
    ensureQuickShellPreviewCompositeSurfaceInitialized();
    state_.deferredQuickShellStartupStageMediaChartPath_ = state_.currentFilePath_;
    // Phase 4c — pick up the &video= override from the parsed document
    // so the lazy host-init path (this code) honours it on first load.
    state_.deferredQuickShellStartupStageMediaVideoOverride_ = session_.applicationServices_.workspace().document().videoPath;
    state_.deferredQuickShellStartupStageMediaPausedSecond_ = qMax(0.0, state_.pauseSecond_);
    state_.deferredQuickShellStartupStageMediaPending_ = !state_.currentFilePath_.isEmpty();
    if (!shouldDeferQuickShellStartupStageMediaLoad()) {
        state_.deferredQuickShellStartupStageMediaPending_ = false;
        state_.previewStageMediaHost_->setChartPath(
            state_.currentFilePath_,
            state_.deferredQuickShellStartupStageMediaVideoOverride_);
        state_.previewStageMediaHost_->setPlayheadSeconds(state_.pauseSecond_);
    }
    refreshQuickShellPreviewCompositeSurfaceState();
    refreshPreviewStageMediaRouteDebugState(false);
}

void miacode::runtime::StageMediaHost::shutdownPreviewStageMediaHost()
{
    if (state_.previewStageMediaHost_ == nullptr) {
        return;
    }
    if (state_.quickShellPreviewCompositeSurface_ != nullptr) {
        state_.quickShellPreviewCompositeSurface_->setMediaHost(nullptr);
        state_.quickShellPreviewCompositeSurface_->setActive(false);
    }
    state_.quickShellPreviewCompositeSurfaceActive_ = false;
    delete state_.previewStageMediaHost_;
    state_.previewStageMediaHost_ = nullptr;
}

void miacode::runtime::StageMediaHost::refreshPreviewStageMediaRouteDebugState(bool requestUpdate)
{
    miacode::runtime::shared::refreshPreviewStageMediaRouteDebugState(state_, requestUpdate);
}

void Session::applyPreviewStageMediaRouteVisualSettings()
{
    stageMedia_->applyPreviewStageMediaRouteVisualSettings();
}

Session::PreviewStageMediaRoute Session::previewStageMediaRoute() const
{
    return stageMedia_->previewStageMediaRoute();
}

bool Session::previewUsesStageMediaHostRoute() const
{
    return stageMedia_->previewUsesStageMediaHostRoute();
}

bool Session::quickShellPreviewUsesSeparateSurface() const
{
    return stageMedia_->quickShellPreviewUsesSeparateSurface();
}

QWindow* Session::quickShellPreviewCompositeWindow() const
{
    return stageMedia_->quickShellPreviewCompositeWindow();
}

bool Session::shouldDeferQuickShellStartupStageMediaLoad() const
{
    return stageMedia_->shouldDeferQuickShellStartupStageMediaLoad();
}

void Session::noteQuickShellStartupUiReady()
{
    stageMedia_->noteQuickShellStartupUiReady();
}

void Session::scheduleDeferredQuickShellStartupStageMediaLoadIfReady()
{
    stageMedia_->scheduleDeferredQuickShellStartupStageMediaLoadIfReady();
}

void Session::updatePreviewStageMediaPresentationMode(bool requestUpdate)
{
    stageMedia_->updatePreviewStageMediaPresentationMode(requestUpdate);
}

void Session::ensurePreviewStageMediaRouteInitialized()
{
    stageMedia_->ensurePreviewStageMediaRouteInitialized();
}

void Session::syncPreviewStageMediaRouteChartPath(const QString& chartPath, const QString& trackPath, double pausedSecond, const QString& chartVideoOverridePath)
{
    stageMedia_->syncPreviewStageMediaRouteChartPath(chartPath, trackPath, pausedSecond, chartVideoOverridePath);
}

void Session::clearPreviewStageMediaRoute()
{
    stageMedia_->clearPreviewStageMediaRoute();
}

void Session::releasePreviewStageMediaDecoderForFileOperation()
{
    stageMedia_->releasePreviewStageMediaDecoderForFileOperation();
}

void Session::applyPreviewStageMediaRoutePlaybackRate(double rate, const char* site)
{
    stageMedia_->applyPreviewStageMediaRoutePlaybackRate(rate, site);
}

double Session::previewStageMediaRouteCurrentPlaybackSecond() const
{
    return stageMedia_->previewStageMediaRouteCurrentPlaybackSecond();
}

void Session::startPreviewStageMediaRoutePlayback(double second)
{
    stageMedia_->startPreviewStageMediaRoutePlayback(second);
}

void Session::ensureQuickShellPreviewCompositeSurfaceInitialized()
{
    stageMedia_->ensureQuickShellPreviewCompositeSurfaceInitialized();
}

void Session::refreshQuickShellPreviewCompositeSurfaceState()
{
    stageMedia_->refreshQuickShellPreviewCompositeSurfaceState();
}

void Session::ensurePreviewStageMediaHostInitialized()
{
    stageMedia_->ensurePreviewStageMediaHostInitialized();
}

void Session::shutdownPreviewStageMediaHost()
{
    stageMedia_->shutdownPreviewStageMediaHost();
}

void Session::refreshPreviewStageMediaRouteDebugState(bool requestUpdate)
{
    stageMedia_->refreshPreviewStageMediaRouteDebugState(requestUpdate);
}
