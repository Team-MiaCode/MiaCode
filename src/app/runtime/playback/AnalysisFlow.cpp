#include "app/runtime/playback/PlaybackCoordinator.h"

#include "app/services/ApplicationServices.h"
#include "common/ProcessDiagnostics.h"
#include "app/runtime/playback/TimelineFlow.Internal.h"
#include "timeline/TimelineAnalysisPublication.h"

#include <QElapsedTimer>
#include <QTimer>

using namespace miacode::runtime::preview_timeline_detail;


void miacode::runtime::PlaybackCoordinator::scheduleTimelineAnalysisRefresh(
    const TimelineSlowRefreshRequest& request)
{
    state_.timeline_.pendingTimelineAnalysisRefresh_ = request;
    state_.timeline_.timelineAnalysisRequestedRevision_ = request.revision;
    requestTimelineAnalysisDispatch();
}

bool miacode::runtime::PlaybackCoordinator::scheduleTimelineAnalysisRefreshFromLatestPreviewState(int delayMs)
{
    if (!hasActiveDifficulty()
        || !state_.timeline_.latestTimelinePreviewSnapshotReady_
        || state_.timeline_.lastTimelineParseDifficultyId_ != activeDifficultyId()
        || state_.timeline_.lastTimelineParseChartText_ != activeChartText()
        || state_.timeline_.lastTimelineParseTimingMetadata_ != currentTimingMetadata()) {
        return false;
    }

    TimelineSlowRefreshRequest request;
    request.revision = state_.timeline_.latestTimelinePreviewRevision_;
    request.difficultyId = activeDifficultyId();
    request.chartText = state_.timeline_.lastTimelineParseChartText_;
    request.timingMetadata = state_.timeline_.lastTimelineParseTimingMetadata_;
    request.validationLocale = uiValidationLocale();

    request.firstSeconds = parsedFirstSeconds();
    state_.timeline_.pendingTimelineAnalysisRefresh_ = request;
    state_.timeline_.timelineAnalysisRequestedRevision_ = request.revision;
    requestTimelineAnalysisDispatch(delayMs);
    return true;
}

void miacode::runtime::PlaybackCoordinator::requestTimelineAnalysisDispatch(int delayMs)
{
    if (state_.timeline_.pendingTimelineAnalysisRefresh_.revision == 0 || state_.playing_) return;
    if (delayMs > 0) {
        QTimer::singleShot(delayMs, &owner_, [this]() {
            if (identity_.active()) dispatchTimelineAnalysisRefresh();
        });
    } else {
        dispatchTimelineAnalysisRefresh();
    }
}

void miacode::runtime::PlaybackCoordinator::dispatchTimelineAnalysisRefresh()
{
    if (!hasActiveDifficulty() || state_.playing_ || state_.latencySandboxAuditionActive_
        || state_.timeline_.pendingTimelineAnalysisRefresh_.revision == 0) return;
    const TimelineSlowRefreshRequest request = state_.timeline_.pendingTimelineAnalysisRefresh_;
    const miacode::AnalysisSnapshot snapshot = services_.analysis().snapshot();
    const miacode::ParsedChartSnapshot& parsed = services_.analysis().parsedSnapshot();
    if (!snapshot.available || snapshot.pending || snapshot.revision != services_.workspace().snapshot().revision
        || snapshot.difficultyId != request.difficultyId || !parsed.available
        || parsed.revision != snapshot.revision || parsed.chartText != request.chartText
        || parsed.timingMetadata != request.timingMetadata || parsed.firstSeconds != request.firstSeconds
        || snapshot.noteMarkerSignature != state_.timeline_.latestTimelineNoteMarkerSignature_) return;
    state_.timeline_.pendingTimelineAnalysisRefresh_ = TimelineSlowRefreshRequest();
    if (request.revision != state_.timeline_.timelineAnalysisRequestedRevision_
        || request.revision != state_.timeline_.timelineRevision_ || request.difficultyId != activeDifficultyId()
        || request.chartText != activeChartText() || request.timingMetadata != currentTimingMetadata()
        || request.firstSeconds != parsedFirstSeconds()) return;

    TimelineAnalysisRefreshResult result;
    result.revision = request.revision;
    result.difficultyId = snapshot.difficultyId;
    result.chartText = request.chartText;
    result.validationLocale = snapshot.locale;
    result.timingMetadata = request.timingMetadata;
    result.noteMarkerSignature = snapshot.noteMarkerSignature;
    result.validationReport = snapshot.validation;
    result.analysisReport = snapshot.muri;
    result.staticReferences = snapshot.muriStaticReferences;
    miacode::diag::MemoryStageScope memScope("preview/mem_stage", "analysis_apply");
    QElapsedTimer applyTimer;
    applyTimer.start();
    Session::ValidationCacheEntry entry;
    entry.chartText = result.chartText;
    entry.validationLocale = result.validationReport.issues.isEmpty() ? uiValidationLocale() : result.validationLocale;
    entry.timingMetadata = result.timingMetadata;
    entry.validationRevision = result.revision;
    entry.ok = result.validationReport.ok;
    entry.errorCount = result.validationReport.errorCount;
    entry.warningCount = result.validationReport.warningCount;
    entry.strictNoteCount = result.validationReport.strictNoteCount;
    entry.strictErrorCount = result.validationReport.strictErrorCount;
    entry.issues.reserve(result.validationReport.issues.size());
    for (const SimaiValidationIssue& issue : result.validationReport.issues) {
        Session::ValidationCachedIssue cachedIssue;
        cachedIssue.line = issue.line;
        cachedIssue.col = issue.col;
        cachedIssue.endCol = issue.endCol;
        cachedIssue.severity = issue.severity;
        cachedIssue.rawMessage = issue.rawMessage;
        cachedIssue.displayMessage = issue.displayMessage;
        entry.issues.append(cachedIssue);
    }
    const int validationIssueCount = entry.issues.size();
    const int muriDiagnosticCount = result.analysisReport.diagnostics.size();
    const int muriStaticReferenceCount = result.staticReferences.size();
    miacode::timeline::publishTimelineAnalysisState(
        [&] {
            state_.validationCacheByDifficulty_[result.difficultyId] = std::move(entry);
            state_.pendingDeferredValidationUiRefresh_ = true;
        },
        [&] {
            state_.muriAnalysisReport_ = std::move(result.analysisReport);
            state_.muriAnalysisReport_.revision = ++state_.muriAnalysisReportRevisionCounter_;
            state_.muriAnalysisReportNoteMarkerSignature_ = result.noteMarkerSignature;
            state_.muriAnalysisReportDifficultyId_ = result.difficultyId;
            state_.muriAnalysisReportTimelineRevision_ = result.revision;
            state_.muriAnalysisResultAvailable_ = true;
        },
        [&] {
            state_.muriStaticReferences_ = std::move(result.staticReferences);
            state_.muriStaticReferencesNoteMarkerSignature_ = result.noteMarkerSignature;
            state_.muriStaticReferencesDifficultyId_ = result.difficultyId;
            state_.muriStaticReferencesTimelineRevision_ = result.revision;
            state_.muriStaticReferencesAvailable_ = true;
            state_.pendingDeferredMuriUiRefresh_ = true;
        },
        [&] { validation_.notifyDocumentValidationChanged(); });
    if (!state_.playing_) {
        validation_.applyDeferredAnalysisUiUpdates();
    }
    if (state_.runtimeDebugOutputEnabled_) {
        appendTimelinePerfLog(
            QStringLiteral("edit/muri_perf"),
            QStringLiteral("phase=analysis_apply validation_issues=%1 diagnostics=%2 static_refs=%3 elapsed_ms=%4")
                .arg(validationIssueCount)
                .arg(muriDiagnosticCount)
                .arg(muriStaticReferenceCount)
                .arg(applyTimer.nsecsElapsed() / 1000000.0, 0, 'f', 3)
        );
    }
}
