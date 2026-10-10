#include "app/runtime/document/DocumentSessionHost.h"
#include "app/runtime/Shared.h"
#include "app/runtime/editor/EditorHost.h"

#include "app/services/PlaybackStateAuthority.h"

#include "audio/QtPreviewSfxRuntime.h"
#include "core/chart/parser/SimaiParser.h"
#include "app/quick_shell/QuickShellPreviewCompositeSurface.h"
#include "app/quick_shell/QuickShellPreviewSurfacePolicy.h"
#include "core/chart/ChartAssetPaths.h"
#include "common/DebugLog.h"
#include "common/DebugOptions.h"
#include "preview/runtime/PreviewRuntime.h"
#include "preview/stage_media/PreviewStageMediaHost.h"
#include "core/scene/PreviewProgressStatsCache.h"
#include "core/chart/transform/ChartBatchTransform.h"
#include "core/chart/transform/ChartNormalization.h"
#include "timeline/quick/TimelineQuickStateBridge.h"
#include "app/services/ExportPagePort.h"
#include "app/runtime/latency/LatencySandboxController.h"
#include "core/analysis/MuriAnalyzer.h"
#include "core/analysis/MuriPanelEntries.h"
#include "core/analysis/MuriStaticChecker.h"

#include <QtCore>
#include <QtGui>

#include <initializer_list>

using namespace miacode::runtime::shared;

bool miacode::runtime::DocumentSessionHost::deleteDifficultyField(int difficultyId)
{
    const SimaiDifficultyData* difficultyData = session_.applicationServices_.workspace().document().difficulty(difficultyId);
    if (!SimaiDocument::isDifficultyId(difficultyId) || difficultyData == nullptr) {
        return false;
    }

    const bool deletingActiveDifficulty = (difficultyId == state_.activeDifficultyId_);
    const QString currentLevel = difficultyData->level;
    const QString currentDesigner = difficultyData->designer;
    const QString currentChart = deletingActiveDifficulty ? session_.editorText() : difficultyData->chart;

    clearDeletedDifficultyUndoState();
    state_.deletedDifficultyUndoState_.valid = true;
    state_.deletedDifficultyUndoState_.wasActive = deletingActiveDifficulty;
    state_.deletedDifficultyUndoState_.difficultyId = difficultyId;
    state_.deletedDifficultyUndoState_.difficultyData.id = difficultyId;
    state_.deletedDifficultyUndoState_.difficultyData.level = currentLevel;
    state_.deletedDifficultyUndoState_.difficultyData.designer = currentDesigner;
    state_.deletedDifficultyUndoState_.difficultyData.chart = currentChart;

    session_.stopQtPreviewPlayback(true);
    if (!session_.applicationServices_.workspace().removeDifficulty(difficultyId)) {
        return false;
    }
    state_.validationCacheByDifficulty_.remove(difficultyId);
    if (deletingActiveDifficulty) {
        session_.invalidateDocumentValidationRevision();
    } else {
        emit session_.documentValidationChanged();
    }
    state_.documentDirty_ = true;

    if (deletingActiveDifficulty) {
        state_.currentFieldDirty_ = false;
        const QVector<int> remainingIds = session_.applicationServices_.workspace().document().difficultyIds();
        if (remainingIds.isEmpty()) {
            state_.activeDifficultyId_ = 0;
            state_.activeOutlineKey_ = "welcome";
            setChartBottomTabsMode(false);
            clearTimelineAndPreview();
        } else {
            int fallbackId = remainingIds.constFirst();
            int bestDistance = qAbs(fallbackId - difficultyId);
            for (int id : remainingIds) {
                const int distance = qAbs(id - difficultyId);
                if (distance < bestDistance || (distance == bestDistance && id < fallbackId)) {
                    fallbackId = id;
                    bestDistance = distance;
                }
            }
            state_.activeOutlineKey_ = "chart";
            switchToDifficultyField(fallbackId);
        }
    }

    updateDirtyState();
    if (state_.currentFilePath_.isEmpty()) {
        return true;
    }
    saveToPath(state_.currentFilePath_);
    return true;
}

void miacode::runtime::DocumentSessionHost::setChartBottomTabsMode(bool enabled)
{
    session_.setBottomTabsTabVisible(Session::BottomTabsTabId::Timeline, enabled);
    session_.setBottomTabsTabVisible(Session::BottomTabsTabId::Validation, enabled);
    session_.setBottomTabsTabVisible(Session::BottomTabsTabId::Muri, enabled);

    if (enabled) {
        session_.setCurrentBottomTabsTabId(Session::BottomTabsTabId::Timeline);
    }
}

bool miacode::runtime::DocumentSessionHost::switchToLatencyField()
{
    if (ui_.qmlExportSession_ != nullptr) {
        ui_.qmlExportSession_->leave();
    }
    // Preserve the current preview position across the switch, just like
    // switchToDifficultyField does, so entering the latency page keeps the
    // playhead instead of snapping to 0. installSandboxScene() consumes
    // pauseSecond_ (clamped to the test chart duration).
    const double restorePreviewSecond = qMax(0.0, state_.playing_
        ? session_.currentPreviewAuthoritativeAudioClockSecond()
        : state_.pauseSecond_);
    session_.stopQtPreviewPlayback(true);
    state_.pendingPreviewPlaybackStart_ = false;
    state_.pendingPreviewPlaybackResumeFromPause_ = false;
    state_.pendingPreviewPlaybackRevision_ = 0;
    state_.pendingPreviewPlaybackDifficultyId_ = 0;
    state_.pendingPreviewPlaybackSecond_ = 0.0;
    // Non-command write: a page switch relocating the paused playhead, not a
    // seek — see PlaybackStateAuthority.h.
    if (auto* authority = session_.applicationServices_.playbackStateAuthority(); authority != nullptr) {
        authority->repositionSilently(restorePreviewSecond, "switch_to_latency_field");
    }
    state_.activeDifficultyId_ = 0;
    state_.activeOutlineKey_ = "latency";
    setChartBottomTabsMode(true);
    session_.syncPreviewStageMediaRouteChartPath(
        state_.currentFilePath_,
        state_.lastTrackPath_,
        restorePreviewSecond,
        session_.applicationServices_.workspace().document().videoPath);
    if (state_.scene_ != nullptr) {
#ifdef HAVE_QT_MULTIMEDIA
        state_.scene_->setStageMediaAvailable(
            miacode::chart_assets::hasBackgroundMedia(state_.currentFilePath_));
#else
        state_.scene_->setStageMediaAvailable(
            miacode::chart_assets::hasBackgroundMedia(state_.currentFilePath_, false));
#endif
    }
    session_.refreshWaveformCache();
    session_.clearValidationDecorations();
    state_.currentFieldDirty_ = false;
    updateDirtyState();
    session_.updateWindowTitle();
    if (auto* sandbox = session_.latencySandboxController(); sandbox != nullptr) {
        sandbox->setOnPage(true);
    }
    return true;
}

bool miacode::runtime::DocumentSessionHost::switchToExportField()
{
    performSwitchToExportField();
    return state_.activeOutlineKey_ == QLatin1String("export");
}

void miacode::runtime::DocumentSessionHost::performSwitchToExportField()
{
    const int previousActiveDifficultyId = state_.activeDifficultyId_;
    // 编辑与导出页面继承当前谱面时间，安装目标场景时消费此位置。
    state_.exportPreviewEntrySeedSecond_ = qMax(0.0, state_.playing_
              ? session_.currentPreviewAuthoritativeAudioClockSecond()
              : state_.pauseSecond_);
    if (auto* sandbox = session_.latencySandboxController(); sandbox != nullptr) {
        sandbox->setOnPage(false);
    }
    if (ui_.qmlExportSession_ != nullptr) {
        ui_.qmlExportSession_->leave();
    }
    session_.stopQtPreviewPlayback(true);
    state_.pendingPreviewPlaybackStart_ = false;
    state_.pendingPreviewPlaybackResumeFromPause_ = false;
    state_.pendingPreviewPlaybackRevision_ = 0;
    state_.pendingPreviewPlaybackDifficultyId_ = 0;
    state_.pendingPreviewPlaybackSecond_ = 0.0;
    state_.activeDifficultyId_ = 0;
    state_.activeOutlineKey_ = "export";
    setChartBottomTabsMode(false);
    session_.clearValidationDecorations();
    state_.currentFieldDirty_ = false;
    updateDirtyState();
    session_.updateWindowTitle();
    if (ui_.qmlExportSession_ != nullptr) {
        ui_.qmlExportSession_->enter(previousActiveDifficultyId);
    }
}

bool miacode::runtime::DocumentSessionHost::switchToMetadataField()
{
    // 信息页沿用工作区的谱面预览源；延迟测试源在安装谱面前退出。
    const int previewDifficultyId = session_.hasActiveDifficulty()
        ? state_.activeDifficultyId_
        : session_.applicationServices_.workspace().snapshot().activeDifficultyId;
    if (!session_.hasActiveDifficulty()
        && SimaiDocument::isDifficultyId(previewDifficultyId)
        && session_.applicationServices_.workspace().document().difficulty(previewDifficultyId) != nullptr) {
        if (!switchToDifficultyField(previewDifficultyId)) {
            return false;
        }
    }
    if (auto* sandbox = session_.latencySandboxController(); sandbox != nullptr) {
        sandbox->setOnPage(false);
    }
    if (ui_.qmlExportSession_ != nullptr) {
        ui_.qmlExportSession_->leave();
    }
    session_.stopQtPreviewPlayback(true);
    state_.pendingPreviewPlaybackStart_ = false;
    state_.pendingPreviewPlaybackResumeFromPause_ = false;
    state_.pendingPreviewPlaybackRevision_ = 0;
    state_.pendingPreviewPlaybackDifficultyId_ = 0;
    state_.pendingPreviewPlaybackSecond_ = 0.0;
    state_.activeOutlineKey_ = "metadata";
    setChartBottomTabsMode(false);
    session_.clearValidationDecorations();
    state_.currentFieldDirty_ = false;
    updateDirtyState();
    session_.updateWindowTitle();
    return true;
}

bool miacode::runtime::DocumentSessionHost::switchToWelcomePage()
{
    if (auto* sandbox = session_.latencySandboxController(); sandbox != nullptr) {
        sandbox->setOnPage(false);
    }
    if (ui_.qmlExportSession_ != nullptr) {
        ui_.qmlExportSession_->leave();
    }
    session_.stopQtPreviewPlayback(true);
    state_.pendingPreviewPlaybackStart_ = false;
    state_.pendingPreviewPlaybackResumeFromPause_ = false;
    state_.pendingPreviewPlaybackRevision_ = 0;
    state_.pendingPreviewPlaybackDifficultyId_ = 0;
    state_.pendingPreviewPlaybackSecond_ = 0.0;
    state_.activeDifficultyId_ = 0;
    state_.activeOutlineKey_ = "welcome";
    setChartBottomTabsMode(false);
    session_.clearValidationDecorations();
    state_.currentFieldDirty_ = false;
    updateDirtyState();
    session_.updateWindowTitle();
    return true;
}

bool miacode::runtime::DocumentSessionHost::clearEditorPresentation()
{
    if (auto* sandbox = session_.latencySandboxController(); sandbox != nullptr) {
        sandbox->setOnPage(false);
    }
    if (ui_.qmlExportSession_ != nullptr) {
        ui_.qmlExportSession_->leave();
    }
    state_.activeDifficultyId_ = 0;
    state_.activeOutlineKey_ = QStringLiteral("chart");
    state_.pendingDifficultySwitchPreviewRestore_ = false;
    state_.pendingDifficultySwitchPreviewRestoreDifficultyId_ = 0;
    state_.pendingDifficultySwitchPreviewRestoreSecond_ = 0.0;
    setChartBottomTabsMode(false);
    session_.clearValidationDecorations();
    state_.currentFieldDirty_ = false;
    clearTimelineAndPreview();
    updateDirtyState();
    session_.updateWindowTitle();
    return true;
}

bool miacode::runtime::DocumentSessionHost::switchToDifficultyField(int difficultyId)
{
    if (!SimaiDocument::isDifficultyId(difficultyId) || session_.applicationServices_.workspace().document().difficulty(difficultyId) == nullptr) {
        return false;
    }
    // The user-facing toggle for this was removed in beta59 — behavior is
    // now always "preserve editor position + preview progress when an
    // active difficulty was selected before the switch" inside the same
    // open document. Opening or closing a document sets
    // resetWorkingPositionPending_ so this restore does not carry the
    // outgoing chart's playhead into the incoming one.
    // Also preserve when coming FROM the latency page OR the export page: both set
    // activeDifficultyId_=0 (so hasActiveDifficulty() is false) but maintain a valid
    // playhead in pauseSecond_ (export audition mirrors the latency
    // sandbox), which we want to carry over to the difficulty. (Both audition flags
    // are still true here — onPageLeft() below tears them down only afterwards.)
    // The metadata (谱面信息) page has no audition either, but leaving a difficulty
    // for it stops playback with keepPosition=true, so pauseSecond_ still
    // holds the last position — carry it back too. Detect the source page from the
    // stack (currentWidget is still the page we're LEAVING — this function switches
    // it to chartPage_ later). Widgets outline writes the destination key before
    // calling us; QML leaveOverlayPage does not, so activeOutlineKey_ can still be
    // "export" or "latency" here and cannot be used as the source. A stale
    // cross-file value is guarded by resetWorkingPositionPending_ when
    // documentOpenGeneration advances.
    const bool leavingMetadataPage = state_.activeOutlineKey_ == QLatin1String("metadata");
    const bool leavingOverlayField = state_.activeOutlineKey_ == QLatin1String("export")
        || state_.activeOutlineKey_ == QLatin1String("latency");
    const bool restoreSwitchView = !session_.resetWorkingPositionPending_
        && (session_.hasActiveDifficulty()
            || state_.latencySandboxAuditionActive_
            || state_.exportPreviewAuditionActive_
            || leavingMetadataPage
            || leavingOverlayField);
    session_.resetWorkingPositionPending_ = false;
    const double restorePreviewSecond = restoreSwitchView
        ? qMax(0.0, state_.playing_
              ? session_.currentPreviewAuthoritativeAudioClockSecond()
              : state_.pauseSecond_)
        : 0.0;
    if (auto* sandbox = session_.latencySandboxController(); sandbox != nullptr) {
        sandbox->setOnPage(false);
    }
    if (ui_.qmlExportSession_ != nullptr) {
        ui_.qmlExportSession_->leave();
    }
    session_.stopQtPreviewPlayback(true);
    state_.pendingPreviewPlaybackStart_ = false;
    state_.pendingPreviewPlaybackResumeFromPause_ = false;
    state_.pendingPreviewPlaybackRevision_ = 0;
    state_.pendingPreviewPlaybackDifficultyId_ = 0;
    state_.pendingPreviewPlaybackSecond_ = 0.0;
    state_.activeDifficultyId_ = difficultyId;
    state_.projectLastOpenedDifficultyId_ = difficultyId;
    // Widgets outline already wrote "chart". QML leaveOverlayPage resumes through
    // this function with the overlay key still set, and shellExportPageActive()
    // uses that key to choose the preview canvas controls.
    if (state_.activeOutlineKey_.isEmpty()
        || state_.activeOutlineKey_ == QLatin1String("metadata")
        || state_.activeOutlineKey_ == QLatin1String("welcome")
        || state_.activeOutlineKey_ == QLatin1String("export")
        || state_.activeOutlineKey_ == QLatin1String("latency")) {
        state_.activeOutlineKey_ = QStringLiteral("chart");
    }
    const double previousPreviewTrackDurationSeconds = state_.previewTrackDurationSeconds_;
    const std::shared_ptr<const miacode::waveform::WaveformData> previousWaveformData =
        state_.timelineQuickStateBridge_ != nullptr ? state_.timelineQuickStateBridge_->waveformData() : nullptr;
    clearTimelineAndPreview(restoreSwitchView);
    if (restoreSwitchView) {
        // 同一工程内切换难度时保留播放时间与时间轴位置，目标谱面解析完成后替换内容。
        if (auto* authority = session_.applicationServices_.playbackStateAuthority(); authority != nullptr) {
            authority->repositionSilently(restorePreviewSecond, "switch_to_difficulty_field");
        }
        state_.pendingDifficultySwitchPreviewRestore_ = true;
        state_.pendingDifficultySwitchPreviewRestoreDifficultyId_ = difficultyId;
        state_.pendingDifficultySwitchPreviewRestoreSecond_ = restorePreviewSecond;
        if (state_.timelineQuickStateBridge_ != nullptr) {
            state_.timelineQuickStateBridge_->setPlayheadSeconds(restorePreviewSecond, false);
        }
        if (state_.scene_ != nullptr) {
            state_.scene_->setPlayheadSeconds(restorePreviewSecond, false);
        }
    } else {
        state_.pendingDifficultySwitchPreviewRestore_ = false;
        state_.pendingDifficultySwitchPreviewRestoreDifficultyId_ = 0;
        state_.pendingDifficultySwitchPreviewRestoreSecond_ = 0.0;
    }
    if (previousWaveformData) {
        session_.applyWaveformData(previousWaveformData);
    } else {
        // Clearing the final editor tab releases the bridge's waveform projection.
        // Entering a difficulty creates a fresh projection from the current track.
        if (previousPreviewTrackDurationSeconds > 0.0) {
            state_.previewTrackDurationSeconds_ = previousPreviewTrackDurationSeconds;
        }
        session_.refreshWaveformCache();
    }
    if (!state_.currentFilePath_.isEmpty()) {
        session_.syncPreviewStageMediaRouteChartPath(
            state_.currentFilePath_,
            state_.lastTrackPath_,
            state_.pauseSecond_,
            session_.applicationServices_.workspace().document().videoPath);  // Phase 4c — &video= override
    }
    setChartBottomTabsMode(true);
    if (state_.timelineQuickStateBridge_ != nullptr) {
        state_.timelineQuickStateBridge_->setFollowPreviewEnabled(state_.previewFollowEnabled_);
    }
    // Entering a difficulty re-asserts the correct preview levels. With the latency
    // audition torn down above (onPageLeft), the mode is Normal, so the single
    // mode-aware dispatch entry pushes the user's real mix (see
    // applyPreviewAudioSettingsToRuntime) — not a special-cased override.
    session_.applyPreviewAudioSettingsToRuntime();
    session_.publishPreviewPlayhead();
    state_.currentFieldDirty_ = false;
    updateDirtyState();
    QTimer::singleShot(0, &session_, [this, difficultyId]() {
        if (state_.activeDifficultyId_ != difficultyId || !difficultyPageActive()) {
            return;
        }
        session_.restoreBottomTabsCurrentTabAfterRefresh(Session::BottomTabsTabId::Timeline);
        session_.scheduleTimelineRefresh();
    });
    session_.saveProjectRenderState();
    // Chart-switch leak gauge. This is the single funnel for BOTH switch paths —
    // loadDocument() reaches a chart only via activateInitialField() -> here — so
    // one call covers difficulty switches and file opens alike. No-ops outside
    // --debug. See emitChartSwitchResourceGauge() for what the sample means.
    session_.emitChartSwitchResourceGauge();
    return true;
}

void miacode::runtime::DocumentSessionHost::activateInitialField()
{
    const QVector<int> ids = session_.applicationServices_.workspace().document().difficultyIds();
    if (!ids.isEmpty()) {
        state_.activeOutlineKey_ = "chart";
        int targetId = 0;
        if (SimaiDocument::isDifficultyId(state_.projectLastOpenedDifficultyId_)
            && ids.contains(state_.projectLastOpenedDifficultyId_)) {
            targetId = state_.projectLastOpenedDifficultyId_;
        }
        if (targetId == 0) {
            const QVector<int> preferredOrder{5, 6, 4, 7, 3, 2, 1};
            targetId = ids.constFirst();
            for (int id : preferredOrder) {
                if (ids.contains(id)) {
                    targetId = id;
                    break;
                }
            }
        }
        switchToDifficultyField(targetId);
    } else {
        state_.activeOutlineKey_ = "welcome";
        switchToWelcomePage();
        clearTimelineAndPreview();
    }
}

void miacode::runtime::DocumentSessionHost::loadDocument()
{
    clearDeletedDifficultyUndoState();
    // Stop the outgoing document before installing the new document state.
    session_.stopQtPreviewPlayback(true);
    const miacode::ChartWorkspaceSnapshot snapshot =
        session_.applicationServices_.workspace().snapshot();
    resetAutosaveState(snapshot.sourceText);
    state_.documentDirty_ = snapshot.dirty;
    state_.currentFieldDirty_ = false;
    state_.activeDifficultyId_ = snapshot.activeDifficultyId;
    if (SimaiDocument::isDifficultyId(state_.activeDifficultyId_)) {
        state_.projectLastOpenedDifficultyId_ = state_.activeDifficultyId_;
    }
    session_.loadProjectValidationPreferences();
    updateDirtyState();
    session_.scheduleTimelineRefresh();
    emit session_.documentReplaced();
}

void miacode::runtime::DocumentSessionHost::clearTimelineAndPreview(bool preservePresentation, bool releaseStorage)
{
    releaseStorage |= !session_.applicationServices_.workspace().snapshot().hasDocument;
    state_.timelineQuickModel_.clear(releaseStorage);
    state_.pendingTimelineSlowRefresh_ = TimelineSlowRefreshRequest();
    state_.pendingTimelineAnalysisRefresh_ = TimelineSlowRefreshRequest();
    state_.timelineSlowRequestedRevision_ = 0;
    state_.timelineAnalysisRequestedRevision_ = 0;
    // The signature describes the markers still installed in the scene. A
    // preserved scene keeps its outgoing markers until the incoming parse is
    // ready, including when that parse produces an empty marker set.
    if (!preservePresentation) {
        state_.lastPreviewNoteMarkerSignature_.clear();
    }
    if (releaseStorage) {
        state_.latestTimelineNoteMarkers_ = QVector<TimelineNoteMarker>();
    } else {
        state_.latestTimelineNoteMarkers_.clear();
    }
    state_.latestTimelineNoteMarkerSignature_.clear();
    state_.latestTimelinePreviewRevision_ = 0;
    state_.latestTimelinePreviewSnapshotReady_ = false;
    state_.lastTimelineParseDifficultyId_ = 0;
    state_.lastTimelineParseChartText_.clear();
    state_.lastTimelineParseTimingMetadata_ = miacode::simai::SimaiTimingMetadata();
    state_.lastTimelineParseResult_ = SimaiParseResult();
    state_.muriAnalysisReport_ = MuriAnalysisReport();
    state_.muriAnalysisReport_.revision = ++state_.muriAnalysisReportRevisionCounter_;
    state_.muriAnalysisReportNoteMarkerSignature_.clear();
    state_.muriAnalysisReportDifficultyId_ = 0;
    state_.muriAnalysisReportTimelineRevision_ = 0;
    state_.muriAnalysisResultAvailable_ = false;
    if (releaseStorage) {
        state_.muriStaticReferences_ = QVector<MuriStaticReference>();
    } else {
        state_.muriStaticReferences_.clear();
    }
    state_.muriStaticReferencesNoteMarkerSignature_.clear();
    state_.muriStaticReferencesDifficultyId_ = 0;
    state_.muriStaticReferencesTimelineRevision_ = 0;
    state_.muriStaticReferencesAvailable_ = false;
    state_.pendingDeferredValidationUiRefresh_ = false;
    state_.pendingDeferredMuriUiRefresh_ = false;
    session_.clearPreviewFollowDecoration();
    session_.clearPreviewObjectStats();
    state_.previewTrackDurationSeconds_ = 0.0;
    state_.qtPreviewTimelineDirty_ = false;
    state_.qtPreviewPendingTimelineSecond_ = 0.0;
    state_.qtPreviewPendingTimelineCenterView_ = true;
    state_.previewFollowBindingCacheValid_ = false;
    state_.previewFollowBindingCache_ = TimelineQuickModel::PreviewFollowBinding();
    state_.pendingQuickTimelineCursorSync_ = false;
    state_.pendingQuickTimelineCursorSecond_ = 0.0;
    state_.pendingQuickTimelineCursorCenterView_ = false;
    state_.pendingPreviewPlaybackStart_ = false;
    state_.pendingPreviewPlaybackResumeFromPause_ = false;
    state_.pendingPreviewPlaybackRevision_ = 0;
    state_.pendingPreviewPlaybackDifficultyId_ = 0;
    state_.pendingPreviewPlaybackSecond_ = 0.0;
    state_.qtPreviewLastTimelineSecond_ = -1.0;
    state_.qtPreviewTimelineStartSecond_ = 0.0;
    state_.qtPreviewPlaybackReturnSecond_ = 0.0;
    state_.qtPreviewPlaybackEndSecond_ = 0.0;
    if (state_.previewSfxRuntime_ != nullptr) {
        state_.previewSfxRuntime_->clearTimeline();
    }
    session_.stopQtPreviewPlayback(preservePresentation);
    if (state_.timelineQuickStateBridge_ != nullptr) {
        if (!preservePresentation) {
            state_.timelineQuickStateBridge_->clear();
        }
        state_.timelineQuickStateBridge_->setMuriAnalysisReport(state_.muriAnalysisReport_);
    }
    if (state_.scene_ != nullptr) {
        if (!preservePresentation) {
            state_.scene_->reset(releaseStorage);
        }
        state_.scene_->setMuriAnalysisReport(state_.muriAnalysisReport_);
    }
    if (!preservePresentation || state_.currentFilePath_.isEmpty()) {
        session_.clearPreviewStageMediaRoute();
    }
    session_.publishPreviewPlayhead();
}
