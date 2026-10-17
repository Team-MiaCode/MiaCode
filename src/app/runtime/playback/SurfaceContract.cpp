#include "app/runtime/Session.h"
#include "app/runtime/Shared.h"
#include "app/runtime/shell/ShellHost.h"
#include "app/runtime/preview/StageMediaHost.h"
#include "app/runtime/playback/PlaybackCoordinator.h"
#include "app/runtime/playback/PreviewStopOrPlayPolicy.h"
#include "app/runtime/playback/Playback.Internal.h"
#include "app/runtime/validation/ValidationHost.h"

#include "audio/QtPreviewSfxRuntime.h"
#include "common/DebugLog.h"
#include "app/services/HudFontPreferences.h"
#include "core/scene/PreviewProgressStatsCache.h"
#include "preview/runtime/PreviewRuntime.h"
#include "preview/stage_media/PreviewStageMediaHost.h"
#include "timeline/quick/TimelineQuickStateBridge.h"

using namespace miacode::runtime::shared;
using namespace miacode::runtime::playback_detail;


namespace {

constexpr double kScrubMisalignWarnSeconds = 0.05;

}  // namespace

bool miacode::runtime::PlaybackCoordinator::playing() const
{
    return identity_.active() && (state_.playing_ || exportIntroLeadInPlaying());
}

miacode::PlaybackTransportState miacode::runtime::PlaybackCoordinator::playbackTransportState() const
{
    return identity_.active()
        ? state_.previewTransportState_
        : miacode::PlaybackTransportState::Stopped;
}

double miacode::runtime::PlaybackCoordinator::positionSeconds() const
{
    if (!identity_.active()) {
        return 0.0;
    }
    if (state_.exportIntroRegionActive_) {
        return state_.exportIntroPlayheadSeconds_;
    }
    return qMax(0.0, state_.pauseSecond_);
}

double miacode::runtime::PlaybackCoordinator::durationSeconds() const
{
    return identity_.active() ? previewDurationSeconds() : 0.0;
}

double miacode::runtime::PlaybackCoordinator::lowerBoundSeconds() const
{
    return identity_.active() ? exportIntroLowerBoundSeconds() : 0.0;
}

void miacode::runtime::PlaybackCoordinator::togglePlayback()
{
    if (!beginPlaybackCommand()) {
        return;
    }
    onTogglePreviewPause();
}

void miacode::runtime::PlaybackCoordinator::stopOrPlay()
{
    if (!beginPlaybackCommand()) {
        return;
    }
    if (previewStopOrPlayShouldStop(state_.playing_, state_.previewStartupSyncPending_,
                                   state_.previewLateVideoStartPending_)) {
        onStopPreview();
    } else {
        onTogglePreviewPause();
    }
}

void miacode::runtime::PlaybackCoordinator::setPlaybackRangeEnabled(
    bool enabled, double startSecond, double endSecond)
{
    if (!beginPlaybackCommand()) {
        return;
    }
    if (enabled && (!qIsFinite(startSecond) || !qIsFinite(endSecond)
                    || endSecond <= startSecond)) {
        return;
    }
    rangePlaybackStartSeconds_ = enabled ? qMax(0.0, startSecond) : 0.0;
    rangePlaybackEndSeconds_ = enabled ? endSecond : 0.0;
    if (state_.timeline_.timelineQuickStateBridge_ != nullptr) {
        state_.timeline_.timelineQuickStateBridge_->setPlayheadUpperLimitSeconds(
            enabled ? endSecond : previewContentDurationSeconds());
    }
    if (state_.playing_) {
        state_.qtPreviewPlaybackEndSecond_ = enabled ? endSecond : previewContentDurationSeconds();
    }
    if (enabled) {
        const double currentSecond = state_.playing_
            ? authoritativeAudioClockSecond() : state_.pauseSecond_;
        const bool introInRange = rangePlaybackStartSeconds_ <= 1e-6
            && state_.exportIntroRegionActive_;
        if (!introInRange && (currentSecond + 1e-6 < rangePlaybackStartSeconds_
                             || currentSecond + 1e-6 >= rangePlaybackEndSeconds_)) {
            seekPreviewToSecond(rangePlaybackStartSeconds_, true);
        }
    }
}

void miacode::runtime::PlaybackCoordinator::stop()
{
    if (!beginPlaybackCommand()) {
        return;
    }
    onStopPreview();
}

void miacode::runtime::PlaybackCoordinator::seek(double second)
{
    if (!beginPlaybackCommand()) {
        return;
    }
    if (handleExportIntroSliderSeek(second)) {
        return;
    }
    seekPreviewToSecond(second, true);
    if (!state_.playing_ && !exportIntroLeadInPlaying()) {
        playbackState_.previewTransportState_ = miacode::PlaybackTransportState::Paused;
    }
}

void miacode::runtime::PlaybackCoordinator::beginScrub()
{
    if (!beginPlaybackCommand()) {
        return;
    }
    appendQuickShellBackendLog(QStringLiteral("preview_scrub_begin"));
    stopPreviewHeldSeek();
    state_.previewScrubDragging_ = true;
    state_.previewScrubRenderElapsed_.invalidate();
    if (state_.exportIntroPvSegmentActive_) {
        pauseExportIntroAdvance();
    }
    if (state_.playing_) {
        pauseQtPreviewPlaybackExact();
    }
    playbackState_.previewTransportState_ = miacode::PlaybackTransportState::Scrubbing;
}

void miacode::runtime::PlaybackCoordinator::updateScrub(double second, bool centerView)
{
    if (!beginPlaybackCommand()) {
        return;
    }
    if (handleExportIntroSliderSeek(second)) {
        return;
    }
    const double clampedSecond = qBound(0.0, second, previewDurationSeconds());
    appendQuickShellBackendLog(
        QStringLiteral("preview_scrub_update"),
        QString("second=%1 center=%2")
            .arg(clampedSecond, 0, 'f', 6)
            .arg(centerView ? 1 : 0));
    writePreviewPauseSecond(
        playbackState_.pauseSecond_, clampedSecond, playbackState_.playing_, "update_preview_scrub");
    const bool shouldRenderNow =
        !state_.previewScrubRenderElapsed_.isValid()
        || state_.previewScrubRenderElapsed_.elapsed() >= kPreviewScrubRenderIntervalMs;
    if (shouldRenderNow) {
        requestPausedPreviewSeek(clampedSecond, centerView, false, false);
        state_.previewScrubRenderElapsed_.restart();
    } else {
        schedulePreviewSeek(clampedSecond, centerView);
    }
    const double appliedSecond = state_.pausedSeekAppliedVisualSecond_;
    const double misalignDelta = clampedSecond - appliedSecond;
    if (qAbs(misalignDelta) > kScrubMisalignWarnSeconds) {
        appendQuickShellBackendLog(
            QStringLiteral("preview_scrub_misalign"),
            QString("phase=update handle=%1 requested=%2 applied=%3 delta=%4")
                .arg(second, 0, 'f', 6)
                .arg(clampedSecond, 0, 'f', 6)
                .arg(appliedSecond, 0, 'f', 6)
                .arg(misalignDelta, 0, 'f', 6));
    }
}

void miacode::runtime::PlaybackCoordinator::endScrub(double second, bool centerView)
{
    if (!beginPlaybackCommand()) {
        return;
    }
    if (handleExportIntroSliderSeek(second)) {
        stopPreviewHeldSeek();
        state_.previewScrubDragging_ = false;
        state_.previewScrubRenderElapsed_.invalidate();
        return;
    }
    const double clampedSecond = qBound(0.0, second, previewDurationSeconds());
    appendQuickShellBackendLog(
        QStringLiteral("preview_scrub_end"),
        QString("second=%1 center=%2")
            .arg(clampedSecond, 0, 'f', 6)
            .arg(centerView ? 1 : 0));
    stopPreviewHeldSeek();
    state_.previewScrubDragging_ = false;
    state_.previewScrubRenderElapsed_.invalidate();
    writePreviewPauseSecond(
        playbackState_.pauseSecond_, clampedSecond, playbackState_.playing_, "end_preview_scrub");
    if (ui_.previewSeekDebounceTimer_ != nullptr) {
        ui_.previewSeekDebounceTimer_->stop();
    }
    seekPreviewToSecond(clampedSecond, centerView);
    playbackState_.previewTransportState_ = miacode::PlaybackTransportState::Paused;
    const double appliedSecond = state_.pauseSecond_;
    const double misalignDelta = clampedSecond - appliedSecond;
    if (qAbs(misalignDelta) > kScrubMisalignWarnSeconds) {
        appendQuickShellBackendLog(
            QStringLiteral("preview_scrub_misalign"),
            QString("phase=end handle=%1 requested=%2 applied=%3 delta=%4")
                .arg(second, 0, 'f', 6)
                .arg(clampedSecond, 0, 'f', 6)
                .arg(appliedSecond, 0, 'f', 6)
                .arg(misalignDelta, 0, 'f', 6));
    }
}

void miacode::runtime::PlaybackCoordinator::setPlaybackRate(double rate)
{
    if (!beginPlaybackCommand()) {
        return;
    }
    applyPreviewPlaybackRate(rate);
}

void miacode::runtime::PlaybackCoordinator::nudgePlaybackRate(int direction)
{
    if (!beginPlaybackCommand()) {
        return;
    }
    applyPreviewPlaybackRate(steppedPreviewPlaybackRate(state_.previewPlaybackRate_, direction));
}

double miacode::runtime::PlaybackCoordinator::playbackRate() const
{
    return identity_.active() ? state_.previewPlaybackRate_ : 1.0;
}

QString miacode::runtime::PlaybackCoordinator::playbackRateLabel() const
{
    QString rateText = QString::number(state_.previewPlaybackRate_, 'f', 2);
    while (rateText.endsWith('0')) {
        rateText.chop(1);
    }
    if (rateText.endsWith('.')) {
        rateText.chop(1);
    }
    return QStringLiteral("%1x").arg(rateText);
}

QObject* miacode::runtime::PlaybackCoordinator::previewRuntimeObject() const
{
    return state_.scene_;
}

QObject* miacode::runtime::PlaybackCoordinator::stageMediaHostObject() const
{
    return state_.previewStageMediaHost_;
}

bool miacode::runtime::PlaybackCoordinator::previewStageMediaRouteHasVideo() const
{
    return miacode::runtime::shared::previewStageMediaRouteHasVideo(state_);
}

void miacode::runtime::PlaybackCoordinator::pausePreviewStageMediaRoutePlayback()
{
    miacode::runtime::shared::pausePreviewStageMediaRoutePlayback(state_);
}

void miacode::runtime::PlaybackCoordinator::syncPreviewStageMediaRoutePlayback(double second)
{
    miacode::runtime::shared::syncPreviewStageMediaRoutePlayback(state_, second);
}

void miacode::runtime::PlaybackCoordinator::setPreviewStageMediaRouteObservedPlayheadSecond(double second)
{
    miacode::runtime::shared::setPreviewStageMediaRouteObservedPlayheadSecond(state_, second);
}

void miacode::runtime::PlaybackCoordinator::resetPreviewStageMediaRouteTimelineOffset()
{
    miacode::runtime::shared::resetPreviewStageMediaRouteTimelineOffset(state_);
}

void miacode::runtime::PlaybackCoordinator::submitPreviewStageMediaRoutePausedSeek(double second, quint64 generation)
{
    miacode::runtime::shared::submitPreviewStageMediaRoutePausedSeek(state_, qBound(0.0, second, previewDurationSeconds()), generation);
}

void miacode::runtime::PlaybackCoordinator::seekPreviewStageMediaRouteWhilePaused(double second)
{
    miacode::runtime::shared::seekPreviewStageMediaRouteWhilePaused(state_, qBound(0.0, second, previewDurationSeconds()));
}

double miacode::runtime::PlaybackCoordinator::canvasAspectRatio() const
{
    return normalizedPreviewCanvasAspectRatio(state_.previewCanvasAspectRatio_);
}

QStringList miacode::runtime::PlaybackCoordinator::statsTexts() const
{
    if (state_.previewProgressStatsCache_ == nullptr) {
        return {};
    }
    const miacode::preview::scene::PreviewObjectStatsSnapshot stats =
        state_.previewProgressStatsCache_->snapshotAt(qMax(0.0, state_.pauseSecond_));
    const auto fmt = [](const QString& name, int played, int total) {
        return QString("%1  %2/%3")
            .arg(name.leftJustified(5, QChar(' '), true))
            .arg(played)
            .arg(total);
    };
    return QStringList{
        fmt(QStringLiteral("Tap"), stats.tapPlayed, stats.tapTotal),
        fmt(QStringLiteral("Hold"), stats.holdPlayed, stats.holdTotal),
        fmt(QStringLiteral("Slide"), stats.slidePlayed, stats.slideTotal),
        fmt(QStringLiteral("Touch"), stats.touchPlayed, stats.touchTotal),
        fmt(QStringLiteral("Break"), stats.breakPlayed, stats.breakTotal),
        fmt(QStringLiteral("Total"), stats.totalPlayed, stats.totalCount),
    };
}

RenderMode miacode::runtime::PlaybackCoordinator::muriRenderMode() const
{
    return state_.muriRenderOptions_.renderMode;
}

void miacode::runtime::PlaybackCoordinator::setMuriRenderMode(RenderMode mode)
{
    validation_.setMuriRenderMode(mode, true);
}

void miacode::runtime::PlaybackCoordinator::toggleMuriRenderMode()
{
    RenderMode nextMode = RenderMode::MaimuriDxStyle;
    switch (state_.muriRenderOptions_.renderMode) {
    case RenderMode::Native:
        nextMode = RenderMode::EraseByArea;
        break;
    case RenderMode::EraseByArea:
        nextMode = RenderMode::MaimuriDxStyle;
        break;
    case RenderMode::MaimuriDxStyle:
        nextMode = RenderMode::Native;
        break;
    }
    validation_.setMuriRenderMode(nextMode, true);
}

int miacode::runtime::PlaybackCoordinator::muriHandRadiusPx() const
{
    return state_.muriRenderOptions_.handRadiusPx;
}

void miacode::runtime::PlaybackCoordinator::setMuriHandRadiusPx(int radiusPx)
{
    validation_.setMuriHandRadiusPx(radiusPx);
}

int miacode::runtime::PlaybackCoordinator::muriTapOnSlideThresholdMs() const
{
    return state_.staticTapOnSlideThresholdMs_;
}

void miacode::runtime::PlaybackCoordinator::setMuriTapOnSlideThresholdMs(int thresholdMs)
{
    validation_.setStaticTapOnSlideThresholdMs(thresholdMs);
}

QStringList miacode::runtime::PlaybackCoordinator::availableSkinDirectoryNames() const
{
    return availablePreviewSkinDirectoryNames();
}

QString miacode::runtime::PlaybackCoordinator::skinDisplayName(const QString& directoryName) const
{
    return previewSkinDisplayName(directoryName);
}

QString miacode::runtime::PlaybackCoordinator::resolveSkinDir() const
{
    return preview_.resolvePreviewSkinDir();
}

QString miacode::runtime::PlaybackCoordinator::resolveSkinRootDir() const
{
    return resolvePreviewSkinRootDir();
}

QString miacode::runtime::PlaybackCoordinator::resolveCustomOutlineDir() const
{
    return resolvePreviewCustomOutlineDir();
}

QStringList miacode::runtime::PlaybackCoordinator::availableCustomOutlineFileNames() const
{
    return availablePreviewCustomOutlineFileNames();
}

QString miacode::runtime::PlaybackCoordinator::customOutlineDisplayName(const QString& fileName) const
{
    return previewCustomOutlineDisplayName(fileName);
}

QString miacode::runtime::PlaybackCoordinator::currentCustomOutlineFileName() const
{
    return preview_.currentPreviewCustomOutlineFileName();
}

void miacode::runtime::PlaybackCoordinator::applyOutlineVariant(PreviewOutlineVariant variant, bool useAutoSelection,
                                                  bool persistState)
{
    preview_.applyPreviewOutlineVariant(variant, useAutoSelection, persistState);
}

void miacode::runtime::PlaybackCoordinator::applyCustomOutlineFileName(const QString& fileName, bool persistState)
{
    preview_.applyPreviewCustomOutlineFileName(fileName, persistState);
}

QVariantMap miacode::runtime::PlaybackCoordinator::renderSettings() const
{
    return preferences_.previewRenderSettings();
}

void miacode::runtime::PlaybackCoordinator::setRenderSetting(const QString& key, const QVariant& value)
{
    preferences_.setPreviewRenderSetting(key, value);
}

void miacode::runtime::PlaybackCoordinator::refreshSurfaces()
{
    if (state_.scene_ != nullptr) {
        state_.scene_->setHudFontSettings(miacode::hud_preferences::load());
        state_.scene_->update();
    }
}

void miacode::runtime::PlaybackCoordinator::prepareForMediaFileOperation()
{
    if (state_.previewStageMediaHost_ != nullptr) {
        state_.previewStageMediaHost_->setChartPath(QString());
        state_.previewStageMediaHost_->releaseDecoderForFileReplace();
    }
}

void miacode::runtime::PlaybackCoordinator::refreshMediaAfterFileOperation()
{
    if (state_.previewStageMediaHost_ != nullptr && !state_.currentFilePath_.isEmpty()) {
        state_.previewStageMediaHost_->setChartPath(
            state_.currentFilePath_, services_.workspace().document().videoPath);
        state_.previewStageMediaHost_->setPlayheadSeconds(qMax(0.0, state_.pauseSecond_));
    }
    refreshSurfaces();
}

void miacode::runtime::PlaybackCoordinator::applySfxLevels()
{
    if (state_.previewSfxRuntime_ != nullptr && state_.previewSfxRuntime_->audioEngineInitialized()) {
        state_.previewSfxRuntime_->applyLevels(state_.previewAudioSettings_);
    }
}

void miacode::runtime::PlaybackCoordinator::prepareForShutdown()
{
    // Order is load-bearing. preview_.preparePreviewForShutdown() (Session's
    // own orchestration) stops the preview timers and flips the surface
    // flags first; those steps can themselves run direct-connected signal
    // cascades that re-arm a fixed-gate awaiting/queued flag. Only after
    // that settles do we zero the playback-domain fields below, so nothing
    // downstream can set one back to true. A lingering singleShot tick
    // callback that still fires later during teardown then reads them as
    // already-cleared and takes the no-op branch instead of re-entering the
    // request path. Clearing first would let a reentrant call triggered by
    // the forwarded call re-arm a flag we had already zeroed.
    preview_.preparePreviewForShutdown();

    miacode::runtime::shared::writePreviewPlayingFlag(playbackState_, services_.shellNotifications(), false);

    state_.qtPreviewAwaitingFrameSwap_ = false;
    state_.qtPreviewAwaitingFrameSwapSinceMs_ = -1;
    state_.qtPreviewAwaitingFrameSwapSinceNs_ = -1;
    state_.qtPreviewDisplayRefreshTickQueued_ = false;
    state_.qtPreviewFixedAwaitingFrame_ = false;
    state_.qtPreviewFixedAwaitingFrameSinceMs_ = -1;
    state_.qtPreviewFixedAwaitingFrameSinceNs_ = -1;
    state_.qtPreviewFixedFrameTickQueued_ = false;
    state_.qtPreviewLastVisualTickNs_ = -1;
    playbackState_.previewStartupSyncPending_ = false;
    playbackState_.previewLateVideoStartPending_ = false;
    state_.previewStartupVideoPrepareStarted_ = false;
    state_.pausedPreviewMediaSeekPending_ = false;
    state_.pendingPreviewPlaybackStart_ = false;
    state_.activePreviewPlaybackTransactionId_ = 0;
}

PreviewAudioSettings miacode::runtime::PlaybackCoordinator::audioSettings() const
{
    return state_.previewAudioSettings_;
}

void miacode::runtime::PlaybackCoordinator::applyAudioSettings(const PreviewAudioSettings& settings)
{
    preferences_.applyPreviewAudioSettingsFromUi(settings);
}

void miacode::runtime::PlaybackCoordinator::saveAudioSettingsAsSoftwareDefault()
{
    preferences_.savePreviewAudioSettingsAsSoftwareDefault();
}

void miacode::runtime::PlaybackCoordinator::restoreAudioSettingsFromSoftwareDefault()
{
    preferences_.restorePreviewAudioSettingsFromSoftwareDefault();
}

QObject* miacode::runtime::PlaybackCoordinator::timelineStateBridge() const
{
    return static_cast<QObject*>(state_.timeline_.timelineQuickStateBridge_);
}

void miacode::runtime::PlaybackCoordinator::navigateToSecond(double second)
{
    onTimelineHeaderNavigateRequested(second);
}

void miacode::runtime::PlaybackCoordinator::centerOnSecond(double second)
{
    onTimelineCenterNavigateRequested(second);
}

void miacode::runtime::PlaybackCoordinator::wheelNavigateToSecond(double second)
{
    onTimelineWheelNavigateRequested(second);
}

void miacode::runtime::PlaybackCoordinator::timelineDragStarted()
{
    onTimelineDragStarted();
}

void miacode::runtime::PlaybackCoordinator::timelineDragFinished(double second)
{
    onTimelineDragFinished(second);
}

void miacode::runtime::PlaybackCoordinator::timelineUserInteractionStarted()
{
    onTimelineUserInteractionStarted();
}

void miacode::runtime::PlaybackCoordinator::setFollowPreviewEnabled(bool enabled)
{
    onTimelineFollowPreviewToggled(enabled);
}

bool miacode::runtime::PlaybackCoordinator::bottomTabsTabVisibleFromState(
    RuntimeContext::BottomTabsTabId tabId) const
{
    switch (tabId) {
    case RuntimeContext::BottomTabsTabId::Timeline:
        return state_.timelineTabVisible_;
    case RuntimeContext::BottomTabsTabId::Validation:
        return state_.validationTabVisible_;
    case RuntimeContext::BottomTabsTabId::Muri:
        return state_.muriTabVisible_;
    case RuntimeContext::BottomTabsTabId::Unknown:
        break;
    }
    return false;
}

QString miacode::runtime::PlaybackCoordinator::bottomTabsTabIdToString(RuntimeContext::BottomTabsTabId tabId)
{
    switch (tabId) {
    case RuntimeContext::BottomTabsTabId::Timeline:
        return QStringLiteral("timeline");
    case RuntimeContext::BottomTabsTabId::Validation:
        return QStringLiteral("validation");
    case RuntimeContext::BottomTabsTabId::Muri:
        return QStringLiteral("muri");
    case RuntimeContext::BottomTabsTabId::Unknown:
        break;
    }
    return QStringLiteral("unknown");
}

miacode::runtime::RuntimeContext::BottomTabsTabId miacode::runtime::PlaybackCoordinator::bottomTabsTabIdFromString(const QString& tabId)
{
    const QString normalized = tabId.trimmed().toLower();
    if (normalized == QStringLiteral("timeline")) {
        return RuntimeContext::BottomTabsTabId::Timeline;
    }
    if (normalized == QStringLiteral("validation")) {
        return RuntimeContext::BottomTabsTabId::Validation;
    }
    if (normalized == QStringLiteral("muri")) {
        return RuntimeContext::BottomTabsTabId::Muri;
    }
    return RuntimeContext::BottomTabsTabId::Unknown;
}

QString miacode::runtime::PlaybackCoordinator::bottomTabsCurrentTabId() const
{
    return bottomTabsTabIdToString(state_.currentBottomTabsTabId_);
}

void miacode::runtime::PlaybackCoordinator::setBottomTabsCurrentTabId(const QString& tabId)
{
    setCurrentBottomTabsTabId(bottomTabsTabIdFromString(tabId));
}

void miacode::runtime::PlaybackCoordinator::setCurrentBottomTabsTabId(RuntimeContext::BottomTabsTabId tabId)
{
    if (tabId == RuntimeContext::BottomTabsTabId::Unknown) {
        return;
    }
    // In the QML backend, the tab bar owns presentation while this state
    // remains the single source of truth for the selected tab.
    if (!state_.backendActive_ && !bottomTabsTabVisibleFromState(tabId)) {
        return;
    }
    const RuntimeContext::BottomTabsTabId previousTabId = state_.currentBottomTabsTabId_;
    state_.currentBottomTabsTabId_ = tabId;
    if (tabId == RuntimeContext::BottomTabsTabId::Timeline) {
        flushDeferredTimelineBridgeState();
    }
    if (previousTabId != tabId) {
        emit services_.shellNotifications().presentationChanged();
    }
}

bool miacode::runtime::PlaybackCoordinator::bottomTabsVisible() const
{
    return bottomTabsTabVisibleFromState(RuntimeContext::BottomTabsTabId::Timeline)
        || bottomTabsTabVisibleFromState(RuntimeContext::BottomTabsTabId::Validation)
        || bottomTabsTabVisibleFromState(RuntimeContext::BottomTabsTabId::Muri);
}

bool miacode::runtime::PlaybackCoordinator::timelineTabVisible() const
{
    return bottomTabsTabVisibleFromState(RuntimeContext::BottomTabsTabId::Timeline);
}

bool miacode::runtime::PlaybackCoordinator::muriTabVisible() const
{
    return bottomTabsTabVisibleFromState(RuntimeContext::BottomTabsTabId::Muri);
}

bool miacode::runtime::PlaybackCoordinator::validationTabVisible() const
{
    return bottomTabsTabVisibleFromState(RuntimeContext::BottomTabsTabId::Validation);
}

bool miacode::runtime::PlaybackCoordinator::ignoreMuriIssuePrompts() const
{
    return state_.ignoreMuriIssuePrompts_;
}

void miacode::runtime::PlaybackCoordinator::noteTimelineSurfaceReady()
{
    if (state_.timeline_.timelineReady_) {
        return;
    }
    state_.timeline_.timelineReady_ = true;
    if (state_.runtimeDebugOutputEnabled_) {
        miacode::debug_log::appendLine(
            miacode::debug_log::Channel::Runtime,
            QStringLiteral("quick_shell/backend"),
            QStringLiteral("action=quick_timeline_ready pending_cursor_sync=%1 second=%2 center=%3")
                .arg(state_.timeline_.pendingQuickTimelineCursorSync_ ? 1 : 0)
                .arg(state_.timeline_.pendingQuickTimelineCursorSecond_, 0, 'f', 6)
                .arg(state_.timeline_.pendingQuickTimelineCursorCenterView_ ? 1 : 0)
        );
    }
    flushDeferredTimelineBridgeState();
}
