#include "app/runtime/playback/PlaybackCoordinator.h"
#include "app/runtime/ContentDurationConfig.h"
#include "timeline/quick/TimelineQuickStateBridge.h"
#include "app/runtime/playback/TimelineFlow.Internal.h"

#include <QElapsedTimer>

using namespace miacode::runtime::preview_timeline_detail;

bool miacode::runtime::PlaybackCoordinator::refreshTimelineQuickModelFromCurrentText()
{
    if (state_.timeline_.timelineQuickStateBridge_ == nullptr || !hasActiveDifficulty()) {
        return false;
    }
    QElapsedTimer timer;
    timer.start();
    if (!state_.timeline_.timelineQuickModel_.updateFromText(activeChartText(), parsedFirstSeconds(), currentTimingMetadata()))
        return false;
    invalidatePreviewFollowBindingCache();
    const auto& snapshot = state_.timeline_.timelineQuickModel_.snapshot();
    if (state_.pendingDifficultySwitchPreviewRestore_
        && state_.pendingDifficultySwitchPreviewRestoreDifficultyId_ == activeDifficultyId()) {
        const double duration = miacode::content_duration::totalContentDurationSeconds(
            snapshot.durationSeconds, state_.previewTrackDurationSeconds_);
        const double restoredSecond = qBound(0.0, state_.pendingDifficultySwitchPreviewRestoreSecond_, duration);
        repositionSilently(restoredSecond, "switch_timeline_snapshot");
        state_.timeline_.timelineQuickStateBridge_->replaceTimelineData(snapshot, restoredSecond, duration);
    } else {
        state_.timeline_.timelineQuickStateBridge_->setTimelineData(snapshot);
    }
    if (state_.runtimeDebugOutputEnabled_) {
        appendTimelinePerfLog(
            QStringLiteral("edit/quick_timeline_perf"),
            QStringLiteral("mode=update revision=%1 lines=%2 elapsed_ms=%3")
                .arg(state_.timeline_.timelineRevision_ + 1)
                .arg(state_.timeline_.timelineQuickModel_.snapshot().lines.size())
                .arg(timer.nsecsElapsed() / 1000000.0, 0, 'f', 3)
        );
    }
    return true;
}
