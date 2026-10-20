#include "audio/bass/BassPreviewAudioBackend.h"

#include "audio/bass/PreviewBassEmergencyPause.h"

#include "audio/bass/BassPreviewDebugLogRouting.h"
#include "audio/bass/BassPreviewRetainedState.h"
#include "core/chart/ChartAssetPaths.h"
#include "common/DebugLog.h"
#include "common/DebugOptions.h"
#include "common/FileContentStamp.h"
#include "common/OperationLog.h"
#include "audio/PreviewAudioMixConfig.h"
#include "core/scene/PreviewSfxAssets.h"
#include "core/scene/PreviewSfxTimeline.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QtMath>

#include <chrono>
#include <limits>
#include <mutex>
#include <cstdio>   // G1 Commit 8 followup: std::snprintf for startup-beacon lines

#include "bass.h"
#include "bassmix.h"

#include "audio/bass/BassPreviewAudioBackendImpl.h"
#include "audio/bass/BassPreviewAudioBackendSample.h"

using namespace miacode::audio::bass_detail;

void BassPreviewAudioBackend::resetCursor(double second, bool includeCurrentSecond)
{
    MC_OP("BassPreviewAudioBackend::resetCursor");
    bool rearmScheduler = false;
    {
        QMutexLocker locker(&schedulerMutex_);
        rearmScheduler = sfxSchedulerActive_;
    }
    if (rearmScheduler) {
        disarmSfxScheduler("reset_cursor");
    }
    playbackSession_.eventGroupIndex = 0;
    while (playbackSession_.eventGroupIndex < preparedGroups_.size()) {
        const double groupSecond = preparedGroups_[playbackSession_.eventGroupIndex].second;
        const bool beforeStart = includeCurrentSecond
            ? (groupSecond + kBassPreviewEpsilonSeconds < second)
            : (groupSecond <= second + kBassPreviewEpsilonSeconds);
        if (!beforeStart) {
            break;
        }
        ++playbackSession_.eventGroupIndex;
    }
    if (rearmScheduler && playbackSession_.masterRunning) {
        anchorSfxScheduler(second);
    }
}

void BassPreviewAudioBackend::triggerGroup(
    const CollapsedEventGroup& group,
    QString* playedKindsOut,
    TouchholdTransition* touchholdOut,
    miacode::preview_audio::bass::PlayedSfxSnapshot* playedSnapshotOut)
{
    const auto record = [playedKindsOut, playedSnapshotOut](
                            const QString& kind, double gain, bool started) {
        if (!started) {
            return;
        }
        if (playedSnapshotOut != nullptr) {
            playedSnapshotOut->record(kind, gain);
        }
        if (playedKindsOut != nullptr) {
            if (!playedKindsOut->isEmpty()) {
                playedKindsOut->append(QLatin1Char(','));
            }
            playedKindsOut->append(QStringLiteral("%1:%2").arg(kind).arg(gain, 0, 'f', 2));
        }
    };

    for (const Event& event : group.orderedEvents) {
        if (event.kind == QLatin1String("touchhold_start")
            || event.kind == QLatin1String("touchhold_stop")) {
            // Latest-wins ownership: rather than naively start/stop the single
            // shared touch-hold sample per event (which let a prior span's stop
            // clobber the next span's start at a seamless join, and let an older
            // span's stop kill a newer overlapping one), re-derive who should own
            // the voice at this instant and reconcile. Order-independent.
            // touchholdOut is non-null exactly when the caller holds schedulerMutex_, which
            // defers this transition's log line until after the unlock. If a group somehow
            // produces two ownership changes, the slot keeps the last one -- reconcile
            // leaves it untouched when nothing changed -- so the logged row always
            // describes the voice state this group actually ended on.
            reconcileTouchholdVoice(event.second, touchholdOut);
            continue;
        }
        record(event.kind, event.gain, playKindInternal(event.kind, event.gain));
    }

    for (const miacode::preview_sfx_timeline::AggregatedPlayback& playback : group.aggregatedPlaybacks) {
        const double gain = miacode::preview_sfx_timeline::aggregatedPlaybackGain(playback);
        record(playback.kind, gain, playKindInternal(playback.kind, gain));
    }
}

void BassPreviewAudioBackend::drainEvents(double second)
{
    if (playbackSession_.masterRunning && qIsFinite(second)) {
        playbackSession_.lastTickSecond = second;
    }
    serviceSfxScheduler(second, true);
    // A live session is scheduled by the master mixer's decode cursor.  Keeping
    // this fallback only for the pre-commit edge avoids a GUI wake-up replaying
    // the groups that BASS already emitted while the GUI thread was stalled.
    {
        QMutexLocker locker(&schedulerMutex_);
        if (sfxSchedulerActive_) {
            return;
        }
    }
    // G1 Commit 8: bass_sfx_drain per §7.2. Emit one line per tick that actually
    // triggered something, with the chart-second the tick was draining toward,
    // the count, and the first/last group indices. Quiet ticks (drained=0) stay
    // out of the log so the channel isn't dominated by no-ops. Track the range
    // around the loop so we can read it in the log line afterward.
    const int firstIdxBeforeDrain = playbackSession_.eventGroupIndex;
    int drainedCount = 0;
    int lastTriggeredIdx = -1;
    QString playedKinds;
    while (playbackSession_.eventGroupIndex < preparedGroups_.size()) {
        const CollapsedEventGroup& group = preparedGroups_[playbackSession_.eventGroupIndex];
        if (group.second > second + kBassPreviewEpsilonSeconds) {
            break;
        }
        // This is the compatibility backend path. A live BASS session returns
        // above before reaching it, so it cannot duplicate a mixer sync.
        triggerGroup(group, runtimeAudioDebugEnabled() ? &playedKinds : nullptr);
        playbackSession_.lastTriggeredGroupIndex = playbackSession_.eventGroupIndex;
        playbackSession_.lastTriggeredGroupSecond = group.second;
        playbackSession_.triggeredGroupCount += 1;
        sfxLastTriggerMonotonicNs_.store(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count(),
            std::memory_order_relaxed);
        lastTriggeredIdx = playbackSession_.eventGroupIndex;
        ++drainedCount;
        ++playbackSession_.eventGroupIndex;
    }
    if (drainedCount > 0) {
        appendAudioDebugLog(
            QString("bass_sfx_drain at_chart=%1 drained=%2 first_idx=%3 last_idx=%4 played=%5")
                .arg(second, 0, 'f', 6)
                .arg(drainedCount)
                .arg(firstIdxBeforeDrain)
                .arg(lastTriggeredIdx)
                .arg(playedKinds.isEmpty() ? QStringLiteral("(none)") : playedKinds));
    }
}

void BassPreviewAudioBackend::disarmSfxScheduler(const char* reason)
{
    bool wasActive = false;
    bool hadSync = false;
    int groupIndex = -1;
    int removeSyncError = 0;
    quint32 syncToRemove = 0;
    {
        QMutexLocker locker(&schedulerMutex_);
        wasActive = sfxSchedulerActive_;
        hadSync = scheduledGroupSync_ != 0;
        groupIndex = scheduledGroupIndex_;
        // Take ownership of the handle here, but do NOT call into BASS yet -- see below.
        syncToRemove = scheduledGroupSync_;
        scheduledGroupSync_ = 0;
        scheduledGroupIndex_ = -1;
        scheduledMixerAction_ = ScheduledMixerAction::None;
        scheduledGroupTargetPosition_ = 0;
        sfxSchedulerActive_ = false;
        sfxSchedulerAnchorDecodePosition_ = 0;
        sfxClockDivergenceStrikes_ = 0;
    }
    // BASS_ChannelRemoveSync remains OUTSIDE schedulerMutex_. Before A6, the two locks
    // were taken in opposite orders on the two threads that matter:
    //
    //   GUI thread    : schedulerMutex_ -> BASS internal sync lock (inside RemoveSync)
    //   BASS callback : BASS internal sync lock -> schedulerMutex_ (handleMixerGroupSync)
    //
    // RemoveSync waits for an in-flight sync callback to finish. The callback now uses
    // tryLock and defers on contention, which breaks that ABBA cycle; keeping the native
    // removal outside the mutex makes the invariant structural and prevents a future
    // callback change from silently restoring the deadlock.
    //
    // Clearing the scheduler state above is what makes the hoist safe rather than merely
    // narrower: sfxSchedulerActive_ is already false by the time the lock is dropped, so a
    // callback firing in the window between the unlock and the removal takes its own
    // early-out instead of acting on a scheduler that is being torn down.
    if (syncToRemove != 0 && masterMixer_ != 0) {
        BASS_ChannelRemoveSync(masterMixer_, syncToRemove);
        // Read now (BASS keeps only the most recent per-thread code), report below.
        removeSyncError = static_cast<int>(BASS_ErrorGetCode());
    }
    // A callback that lost tryLock() may have published this handle while
    // BASS_ChannelRemoveSync waited for it to return. The disarm owns cancellation, so
    // it also owns discarding that deferred action after the native callback is gone.
    deferredMixerSyncHandle_.store(0, std::memory_order_release);
    drainStaleSyncHandles();
    drainSfxCallbackEvents();
    // Both lines land after the locker's scope ends. The callback no longer waits for
    // this lock, but formatting and I/O still do not belong in a scheduler critical
    // section. The anchor side has always been logged; without the disarm side
    // a session that never re-anchors just stops producing anchor rows, which reads
    // identically to a session that was never armed.
    noteBassErrCode("sfx_scheduler/remove_sync", removeSyncError);
    if (miacode::preview_audio::bass::shouldLogDisarm(wasActive, hadSync, groupIndex)) {
        // The chain counters are cumulative for the backend's lifetime; diffing them
        // between the anchor and this disarm gives the session's chain health at a glance.
        appendAudioDebugLog(
            QString("bass_sfx_scheduler action=disarm reason=%1 was_active=%2 had_sync=%3 group_idx=%4 triggered=%5 inline=%6 skipped=%7 watchdog=%8 deferred=%9")
                .arg(QLatin1String(reason))
                .arg(wasActive ? 1 : 0)
                .arg(hadSync ? 1 : 0)
                .arg(groupIndex)
                .arg(playbackSession_.triggeredGroupCount)
                .arg(sfxInlineTriggerCount_.load(std::memory_order_relaxed))
                .arg(sfxInlineSkipCount_.load(std::memory_order_relaxed))
                .arg(sfxWatchdogRecoveryCount_.load(std::memory_order_relaxed))
                .arg(sfxDeferredSyncCount_.load(std::memory_order_relaxed)));
    }
}

void BassPreviewAudioBackend::anchorSfxScheduler(double chartSecond)
{
    if (masterMixer_ == 0 || !playbackSession_.masterRunning
        || shuttingDown_.load(std::memory_order_acquire)) {
        return;
    }

    disarmSfxScheduler("anchor_rearm");

    int nextGroupIndex = 0;
    bool backgroundPendingStart = false;
    double playbackRate = 1.0;
    SfxSchedulerArmFailure armFailure;
    SfxArmContext context;
    context.source = miacode::preview_audio::bass::SfxArmSource::Anchor;
    QWORD position = static_cast<QWORD>(-1);
    int armedGroupIndex = -1;
    ScheduledMixerAction armedAction = ScheduledMixerAction::None;
    QWORD armedTarget = 0;
    {
        QMutexLocker locker(&schedulerMutex_);
        if (shuttingDown_.load(std::memory_order_acquire)
            || !playbackSession_.masterRunning) {
            return;
        }
        // The cursor is read under the lock, immediately before the first sync is armed.
        // It used to be read outside it: a mixer block rendered in between placed the
        // first target at or behind the cursor, which BASS never delivers, and the chain
        // was dead from its first link. armNextGroupSyncLocked still verifies the result.
        position = BASS_ChannelGetPosition(masterMixer_, BASS_POS_BYTE | BASS_POS_DECODE);
        if (position != static_cast<QWORD>(-1)) {
            sfxSchedulerAnchor_.chartSecond = clampTimelineSecond(chartSecond);
            sfxSchedulerAnchor_.mixerSecond = mixerBytesToSeconds(position);
            sfxSchedulerAnchor_.playbackRate = playbackSession_.backgroundTrackPlaybackRate;
            sfxSchedulerAnchor_.outputBufferSeconds = masterMixerOutputBufferSeconds_;
            sfxSchedulerAnchorDecodePosition_ = position;
            sfxSchedulerActive_ = true;
            sfxClockDivergenceStrikes_ = 0;
            armNextGroupSyncLocked(context);
            armFailure = sfxSchedulerArmFailure_;
            sfxSchedulerArmFailure_ = SfxSchedulerArmFailure();
            nextGroupIndex = playbackSession_.eventGroupIndex;
            backgroundPendingStart = playbackSession_.backgroundTrackPendingStart;
            playbackRate = playbackSession_.backgroundTrackPlaybackRate;
            armedGroupIndex = scheduledGroupIndex_;
            armedAction = scheduledMixerAction_;
            armedTarget = scheduledGroupTargetPosition_;
        }
    }
    if (position == static_cast<QWORD>(-1)) {
        noteBassErr("sfx_scheduler/get_decode_position");
        return;
    }
    finishArmContext(context);
    // `armed_lead_ms` is how far ahead of the cursor the first sync sits; a session that
    // later shows no trigger for `armed_group_idx` with a positive lead here was killed
    // by something after the anchor, not by the anchor itself.
    const double armedLeadMs = scheduledGroupSyncLeadMs(armedTarget, position, armedAction);
    appendAudioDebugLog(
        QString("bass_sfx_scheduler action=anchor chart_second=%1 rate=%2 next_group_idx=%3 bg_pending=%4 decode_pos=%5 armed_group_idx=%6 armed_action=%7 armed_lead_ms=%8 inline=%9 skipped=%10")
            .arg(chartSecond, 0, 'f', 6)
            .arg(playbackRate, 0, 'f', 3)
            .arg(nextGroupIndex)
            .arg(backgroundPendingStart ? 1 : 0)
            .arg(position)
            .arg(armedGroupIndex)
            .arg(scheduledMixerActionLabel(armedAction))
            .arg(armedLeadMs, 0, 'f', 3)
            .arg(context.inlineTriggers)
            .arg(context.inlineSkips));
    // After the anchor row, so the pair reads in the order it happened: the anchor was
    // taken, then arming its first sync failed and the scheduler switched itself off.
    logSfxSchedulerArmFailure(armFailure);
}

double BassPreviewAudioBackend::scheduledGroupSyncLeadMs(
    quint64 targetPosition,
    quint64 decodePosition,
    ScheduledMixerAction action) const
{
    if (action == ScheduledMixerAction::None || decodePosition == static_cast<quint64>(-1)) {
        return 0.0;
    }
    return targetPosition >= decodePosition
        ? mixerBytesToSeconds(targetPosition - decodePosition) * 1000.0
        : -mixerBytesToSeconds(decodePosition - targetPosition) * 1000.0;
}

double BassPreviewAudioBackend::chartSecondForDecodePositionLocked(quint64 decodePosition) const
{
    if (!sfxSchedulerActive_ || masterMixer_ == 0
        || decodePosition == static_cast<quint64>(-1)
        || decodePosition < sfxSchedulerAnchorDecodePosition_) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    const double mixerElapsedSeconds =
        mixerBytesToSeconds(decodePosition - sfxSchedulerAnchorDecodePosition_);
    if (!qIsFinite(mixerElapsedSeconds)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return clampTimelineSecond(
        miacode::preview_audio::bass::chartSecondForMixerSecond(
            sfxSchedulerAnchor_, sfxSchedulerAnchor_.mixerSecond + mixerElapsedSeconds));
}

double BassPreviewAudioBackend::currentSfxSchedulerChartSecond(double fallbackSecond) const
{
    {
        QMutexLocker locker(&schedulerMutex_);
        if (!sfxSchedulerActive_ || masterMixer_ == 0) {
            return fallbackSecond;
        }
    }
    const QWORD currentDecodePosition = BASS_ChannelGetPosition(
        masterMixer_, BASS_POS_BYTE | BASS_POS_DECODE);
    QMutexLocker locker(&schedulerMutex_);
    const double chartSecond = chartSecondForDecodePositionLocked(currentDecodePosition);
    return qIsFinite(chartSecond) ? chartSecond : fallbackSecond;
}

double BassPreviewAudioBackend::liveChartSecondEstimate() const
{
    const double fallback = playbackSession_.masterRunning && playbackSession_.lastTickSecond >= 0.0
        ? playbackSession_.lastTickSecond
        : playbackSession_.lastAuthoritativeSecond;
    return currentSfxSchedulerChartSecond(fallback);
}

void BassPreviewAudioBackend::advanceCursorPastSecondLocked(double second)
{
    int index = qMax(0, playbackSession_.eventGroupIndex);
    while (index < preparedGroups_.size()
           && preparedGroups_[index].second <= second + kBassPreviewEpsilonSeconds) {
        ++index;
    }
    playbackSession_.eventGroupIndex = index;
}

void BassPreviewAudioBackend::armNextGroupSyncLocked(SfxArmContext& context)
{
    using namespace miacode::preview_audio::bass;
    for (int pass = 0; pass < kMaxInlineCatchUpGroups; ++pass) {
        if (!sfxSchedulerActive_ || scheduledGroupSync_ != 0 || masterMixer_ == 0
            || !playbackSession_.masterRunning || shuttingDown_.load(std::memory_order_acquire)) {
            return;
        }

        const int groupIndex = playbackSession_.eventGroupIndex;
        const bool hasGroup = groupIndex >= 0 && groupIndex < preparedGroups_.size();
        const bool hasPendingBackground = backgroundTrackSample_ != nullptr
            && playbackSession_.backgroundTrackPendingStart;
        if (!hasGroup && !hasPendingBackground) {
            return;
        }

        const double groupSecond = hasGroup ? preparedGroups_[groupIndex].second : std::numeric_limits<double>::infinity();
        const double pendingSecond = hasPendingBackground
            ? playbackSession_.backgroundTrackPendingStartSecond
            : std::numeric_limits<double>::infinity();
        const bool startBackgroundFirst = pendingSecond <= groupSecond + kBassPreviewEpsilonSeconds;
        const double targetChartSecond = startBackgroundFirst ? pendingSecond : groupSecond;
        const bool sameInstant = hasGroup && hasPendingBackground
            && qAbs(groupSecond - pendingSecond) <= kBassPreviewEpsilonSeconds;

        const double targetMixerSecond =
            mixerSecondForChartSecond(sfxSchedulerAnchor_, targetChartSecond);
        const double relativeSecond = qMax(0.0, targetMixerSecond - sfxSchedulerAnchor_.mixerSecond);
        const QWORD targetPosition = sfxSchedulerAnchorDecodePosition_
            + BASS_ChannelSeconds2Bytes(masterMixer_, relativeSecond);
        const int actionGroupIndex = startBackgroundFirst && !sameInstant ? -1 : groupIndex;
        const ScheduledMixerAction action = sameInstant
            ? ScheduledMixerAction::SfxGroupAndStartPendingBackgroundTrack
            : (startBackgroundFirst
                ? ScheduledMixerAction::StartPendingBackgroundTrack
                : ScheduledMixerAction::SfxGroup);
        const quint32 syncHandle = BASS_ChannelSetSync(
            masterMixer_,
            BASS_SYNC_POS | BASS_SYNC_MIXTIME | BASS_SYNC_ONETIME,
            targetPosition,
            reinterpret_cast<SYNCPROC*>(BassPreviewAudioBackend::onMixerGroupSync),
            this);
        if (syncHandle == 0) {
            // Live SFX just switched to the GUI drainEvents fallback for the rest of the
            // session — audible, and previously reported only by a bass_err row that
            // noteBassErr suppresses when BASS left no code behind. Recorded rather than
            // logged: this runs under schedulerMutex_, on the BASS mixer thread when the
            // caller is handleMixerGroupSync.
            sfxSchedulerArmFailure_.pending = true;
            sfxSchedulerArmFailure_.bassError = static_cast<int>(BASS_ErrorGetCode());
            sfxSchedulerArmFailure_.targetChartSecond = targetChartSecond;
            sfxSchedulerActive_ = false;
            return;
        }
        // BASS only delivers a position sync when the decode cursor crosses its target; a
        // target the cursor has already reached (the anchor's read raced a mixer block, the
        // group's lead rounded to zero samples, or this pass follows a late replay) would
        // sit in BASS's sync list forever and the chain would end here. Play such a group
        // now and move on to the next; the dead handle is removed outside the lock.
        const QWORD decodeNow = BASS_ChannelGetPosition(masterMixer_, BASS_POS_BYTE | BASS_POS_DECODE);
        if (decodeNow == static_cast<QWORD>(-1) || !armedSyncCannotFire(targetPosition, decodeNow)) {
            scheduledGroupSync_ = syncHandle;
            scheduledGroupTargetPosition_ = targetPosition;
            scheduledGroupIndex_ = actionGroupIndex;
            scheduledMixerAction_ = action;
            return;
        }
        if (context.staleCount < static_cast<int>(context.staleHandles.size())) {
            context.staleHandles[static_cast<std::size_t>(context.staleCount++)] = syncHandle;
        }
        const double lateSeconds = mixerBytesToSeconds(decodeNow - targetPosition);
        SfxCallbackEvent event;
        event.source = context.source;
        event.handle = syncHandle;
        event.decodePosition = decodeNow;
        event.targetPosition = targetPosition;
        event.groupIndex = actionGroupIndex;
        event.groupSecond = targetChartSecond;
        if (lateGroupAction(lateSeconds) == LateGroupAction::TriggerNow) {
            performScheduledActionLocked(actionGroupIndex, action, &event);
            event.kind = SfxCallbackEventKind::InlineTrigger;
            ++context.inlineTriggers;
            sfxInlineTriggerCount_.fetch_add(1, std::memory_order_relaxed);
        } else {
            // The pending BGM is continuous audio: start it late rather than never. The
            // note-sound group itself is past its catch-up window and is skipped.
            if (action != ScheduledMixerAction::SfxGroup) {
                performScheduledActionLocked(-1, ScheduledMixerAction::StartPendingBackgroundTrack, &event);
            }
            if (actionGroupIndex >= 0 && playbackSession_.eventGroupIndex <= actionGroupIndex) {
                playbackSession_.eventGroupIndex = actionGroupIndex + 1;
            }
            event.kind = SfxCallbackEventKind::InlineSkip;
            ++context.inlineSkips;
            sfxInlineSkipCount_.fetch_add(1, std::memory_order_relaxed);
        }
        if (context.eventCount < static_cast<int>(context.events.size())) {
            context.events[static_cast<std::size_t>(context.eventCount++)] = event;
        }
    }
}

void BassPreviewAudioBackend::performScheduledActionLocked(
    int groupIndex,
    ScheduledMixerAction action,
    miacode::preview_audio::bass::SfxCallbackEvent* event)
{
    using namespace miacode::preview_audio::bass;
    const bool startBackground = action == ScheduledMixerAction::StartPendingBackgroundTrack
        || action == ScheduledMixerAction::SfxGroupAndStartPendingBackgroundTrack;
    if (startBackground && backgroundTrackSample_ != nullptr
        && playbackSession_.backgroundTrackPendingStart
        && !playbackSession_.backgroundTrackPastEnd) {
        backgroundTrackSample_->play();
        playbackSession_.backgroundTrackPendingStart = false;
        playbackSession_.backgroundTrackRunning = true;
        event->startedBackground = true;
    }

    const bool shouldTriggerGroup = action == ScheduledMixerAction::SfxGroup
        || action == ScheduledMixerAction::SfxGroupAndStartPendingBackgroundTrack;
    if (shouldTriggerGroup && groupIndex >= 0 && groupIndex < preparedGroups_.size()) {
        const CollapsedEventGroup& group = preparedGroups_[groupIndex];
        if (playbackSession_.eventGroupIndex <= groupIndex) {
            playbackSession_.eventGroupIndex = groupIndex + 1;
        }
        playbackSession_.lastTriggeredGroupIndex = groupIndex;
        playbackSession_.lastTriggeredGroupSecond = group.second;
        ++playbackSession_.triggeredGroupCount;
        TouchholdTransition touchholdTransition;
        triggerGroup(group, nullptr, &touchholdTransition, &event->played);
        sfxLastTriggerMonotonicNs_.store(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count(),
            std::memory_order_relaxed);
        event->kind = SfxCallbackEventKind::Trigger;
        event->groupIndex = groupIndex;
        event->groupSecond = group.second;
        event->triggeredCount = playbackSession_.triggeredGroupCount;
        event->touchholdChanged = touchholdTransition.changed;
        event->touchholdOwner = touchholdTransition.owner;
        event->touchholdPreviousOwner = touchholdTransition.previousOwner;
        event->touchholdSecond = touchholdTransition.second;
        event->touchholdSpanStartSecond = touchholdTransition.spanStartSecond;
    } else if (event->startedBackground) {
        event->kind = SfxCallbackEventKind::Trigger;
    }
}

void BassPreviewAudioBackend::logSfxSchedulerArmFailure(const SfxSchedulerArmFailure& failure) const
{
    if (!failure.pending) {
        return;
    }
    noteBassErrCode("sfx_scheduler/set_sync", failure.bassError);
    appendAudioDebugLog(
        QString("bass_sfx_scheduler action=deactivated reason=set_sync_failed bass_err=%1 target_chart_second=%2")
            .arg(failure.bassError)
            .arg(failure.targetChartSecond, 0, 'f', 6));
}

void BassPreviewAudioBackend::onMixerGroupSync(quint32 handle, quint32 channel, quint32 data, void* user)
{
    Q_UNUSED(channel);
    Q_UNUSED(data);
    auto* backend = static_cast<BassPreviewAudioBackend*>(user);
    if (backend != nullptr) {
        backend->handleMixerGroupSync(handle);
    }
}

void BassPreviewAudioBackend::handleMixerGroupSync(quint32 handle)
{
    if (shuttingDown_.load(std::memory_order_acquire)) {
        return;
    }

    using namespace miacode::preview_audio::bass;
    if (!schedulerMutex_.tryLock()) {
        quint32 emptyHandle = 0;
        deferredMixerSyncHandle_.compare_exchange_strong(
            emptyHandle, handle, std::memory_order_release, std::memory_order_relaxed);
        SfxCallbackEvent event;
        event.kind = SfxCallbackEventKind::Deferred;
        event.handle = handle;
        event.expectedHandle = emptyHandle;
        sfxCallbackEventRing_.tryPush(event);
        return;
    }

    int callbackBassError = 0;
    ScopedRealtimeBassErrorSink errorSink(&callbackBassError);
    SfxCallbackEvent event;
    SfxArmContext context;
    context.source = SfxArmSource::Callback;
    {
        std::lock_guard<QMutex> locker(schedulerMutex_, std::adopt_lock);
        processMixerGroupSyncLocked(handle, false, &event, context);
    }
    event.callbackBassError = callbackBassError;
    if (event.kind != SfxCallbackEventKind::None || callbackBassError != 0) {
        sfxCallbackEventRing_.tryPush(event);
    }
    publishArmContextFromCallback(context);
}

void BassPreviewAudioBackend::processMixerGroupSyncLocked(
    quint32 handle,
    bool processedAfterContention,
    miacode::preview_audio::bass::SfxCallbackEvent* event,
    SfxArmContext& context)
{
    using namespace miacode::preview_audio::bass;
    if (event == nullptr || shuttingDown_.load(std::memory_order_acquire)) {
        return;
    }
    event->handle = handle;
    event->processedAfterContention = processedAfterContention;
    event->source = context.source;
    if (!sfxSchedulerActive_) {
        event->kind = SfxCallbackEventKind::Drop;
        event->dropReason = SfxCallbackDropReason::Inactive;
        event->expectedHandle = scheduledGroupSync_;
        return;
    }
    if (handle == 0 || handle != scheduledGroupSync_) {
        event->kind = SfxCallbackEventKind::Drop;
        event->dropReason = SfxCallbackDropReason::StaleHandle;
        event->expectedHandle = scheduledGroupSync_;
        return;
    }

    const int groupIndex = scheduledGroupIndex_;
    const ScheduledMixerAction action = scheduledMixerAction_;
    event->targetPosition = scheduledGroupTargetPosition_;
    scheduledGroupSync_ = 0;
    scheduledGroupIndex_ = -1;
    scheduledMixerAction_ = ScheduledMixerAction::None;
    scheduledGroupTargetPosition_ = 0;
    // Where the cursor actually is when the sound starts. Equal to the target when BASS
    // delivered the sync itself; the difference is the true lateness of a deferred replay.
    event->decodePosition = BASS_ChannelGetPosition(masterMixer_, BASS_POS_BYTE | BASS_POS_DECODE);

    performScheduledActionLocked(groupIndex, action, event);

    armNextGroupSyncLocked(context);
    event->armFailurePending = sfxSchedulerArmFailure_.pending;
    event->armFailureBassError = sfxSchedulerArmFailure_.bassError;
    event->armFailureTargetChartSecond = sfxSchedulerArmFailure_.targetChartSecond;
    sfxSchedulerArmFailure_ = SfxSchedulerArmFailure();
}

void BassPreviewAudioBackend::finishArmContext(const SfxArmContext& context)
{
    for (int index = 0; index < context.staleCount; ++index) {
        const quint32 handle = context.staleHandles[static_cast<std::size_t>(index)];
        if (handle != 0 && masterMixer_ != 0) {
            BASS_ChannelRemoveSync(masterMixer_, handle);
            noteBassErr("sfx_scheduler/remove_dead_sync");
        }
    }
    for (int index = 0; index < context.eventCount; ++index) {
        logSfxCallbackEvent(context.events[static_cast<std::size_t>(index)]);
    }
}

void BassPreviewAudioBackend::publishArmContextFromCallback(const SfxArmContext& context)
{
    for (int index = 0; index < context.staleCount; ++index) {
        pushStaleSyncHandle(context.staleHandles[static_cast<std::size_t>(index)]);
    }
    for (int index = 0; index < context.eventCount; ++index) {
        sfxCallbackEventRing_.tryPush(context.events[static_cast<std::size_t>(index)]);
    }
}

void BassPreviewAudioBackend::pushStaleSyncHandle(quint32 handle)
{
    if (handle == 0) {
        return;
    }
    for (std::atomic<quint32>& slot : staleSyncHandles_) {
        quint32 expected = 0;
        if (slot.compare_exchange_strong(expected, handle, std::memory_order_acq_rel)) {
            return;
        }
    }
    // Every slot is taken: the handle leaks in BASS's sync list until the master mixer is
    // freed. It can never fire, so this costs memory, not sound.
}

void BassPreviewAudioBackend::drainStaleSyncHandles()
{
    for (std::atomic<quint32>& slot : staleSyncHandles_) {
        const quint32 handle = slot.exchange(0, std::memory_order_acq_rel);
        if (handle != 0 && masterMixer_ != 0) {
            BASS_ChannelRemoveSync(masterMixer_, handle);
            noteBassErr("sfx_scheduler/remove_dead_sync");
        }
    }
}

void BassPreviewAudioBackend::drainDeferredMixerSync()
{
    using namespace miacode::preview_audio::bass;
    const quint32 handle = deferredMixerSyncHandle_.exchange(0, std::memory_order_acq_rel);
    if (handle == 0) {
        return;
    }
    sfxDeferredSyncCount_.fetch_add(1, std::memory_order_relaxed);
    // The sync already fired and BASS removed it; nothing arms the next group until it is
    // processed here. A group still near its note time is played now; one that waited out a
    // stalled worker is skipped, the cursor moved past the live mixer position, and the
    // next group re-armed from the existing anchor (the anchor itself is still right).
    const QWORD decodePosition = masterMixer_ != 0
        ? BASS_ChannelGetPosition(masterMixer_, BASS_POS_BYTE | BASS_POS_DECODE)
        : static_cast<QWORD>(-1);
    int callbackBassError = 0;
    bool skip = false;
    int lateGroupIndex = -1;
    double lateSeconds = 0.0;
    double liveChartSecond = std::numeric_limits<double>::quiet_NaN();
    bool rearmed = false;
    SfxCallbackEvent event;
    SfxArmContext context;
    context.source = SfxArmSource::Deferred;
    {
        ScopedRealtimeBassErrorSink errorSink(&callbackBassError);
        QMutexLocker locker(&schedulerMutex_);
        if (sfxSchedulerActive_ && handle == scheduledGroupSync_
            && decodePosition != static_cast<QWORD>(-1)) {
            lateSeconds = decodePosition >= scheduledGroupTargetPosition_
                ? mixerBytesToSeconds(decodePosition - scheduledGroupTargetPosition_)
                : -mixerBytesToSeconds(scheduledGroupTargetPosition_ - decodePosition);
            skip = !shouldReplayDeferredSync(lateSeconds);
        }
        if (skip) {
            lateGroupIndex = scheduledGroupIndex_;
            const ScheduledMixerAction action = scheduledMixerAction_;
            // Already removed by BASS (one-shot); clearing keeps the disarm from asking
            // BASS to remove a handle that no longer exists.
            scheduledGroupSync_ = 0;
            scheduledGroupIndex_ = -1;
            scheduledMixerAction_ = ScheduledMixerAction::None;
            scheduledGroupTargetPosition_ = 0;
            liveChartSecond = chartSecondForDecodePositionLocked(decodePosition);
            if (action != ScheduledMixerAction::SfxGroup) {
                performScheduledActionLocked(-1, ScheduledMixerAction::StartPendingBackgroundTrack, &event);
            }
            if (qIsFinite(liveChartSecond)) {
                advanceCursorPastSecondLocked(liveChartSecond);
            } else if (lateGroupIndex >= 0 && playbackSession_.eventGroupIndex <= lateGroupIndex) {
                playbackSession_.eventGroupIndex = lateGroupIndex + 1;
            }
            armNextGroupSyncLocked(context);
            rearmed = scheduledGroupSync_ != 0;
            event.armFailurePending = sfxSchedulerArmFailure_.pending;
            event.armFailureBassError = sfxSchedulerArmFailure_.bassError;
            event.armFailureTargetChartSecond = sfxSchedulerArmFailure_.targetChartSecond;
            sfxSchedulerArmFailure_ = SfxSchedulerArmFailure();
        } else {
            processMixerGroupSyncLocked(handle, true, &event, context);
        }
    }
    event.callbackBassError = callbackBassError;
    logSfxCallbackEvent(event);
    finishArmContext(context);
    if (skip) {
        sfxWatchdogRecoveryCount_.fetch_add(1, std::memory_order_relaxed);
        appendAudioDebugLog(
            QString("bass_sfx_scheduler action=recover reason=deferred_sync_late group_idx=%1 late_ms=%2 live_chart=%3 inline=%4 skipped=%5 rearmed=%6")
                .arg(lateGroupIndex)
                .arg(lateSeconds * 1000.0, 0, 'f', 1)
                .arg(liveChartSecond, 0, 'f', 6)
                .arg(context.inlineTriggers)
                .arg(context.inlineSkips)
                .arg(rearmed ? 1 : 0));
    }
}

void BassPreviewAudioBackend::runSfxChainWatchdog(double referenceChartSecond, bool hasReference)
{
    using namespace miacode::preview_audio::bass;
    if (masterMixer_ == 0 || !playbackSession_.masterRunning
        || shuttingDown_.load(std::memory_order_acquire)) {
        sfxClockDivergenceStrikes_ = 0;
        return;
    }
    const QWORD decodePosition = BASS_ChannelGetPosition(
        masterMixer_, BASS_POS_BYTE | BASS_POS_DECODE);
    if (decodePosition == static_cast<QWORD>(-1)) {
        return;
    }
    const QWORD graceBytes = BASS_ChannelSeconds2Bytes(masterMixer_, kMissedSyncGraceSeconds);
    const char* reason = nullptr;
    int groupIndex = -1;
    double lateSeconds = 0.0;
    double schedulerChartSecond = std::numeric_limits<double>::quiet_NaN();
    bool rearmed = false;
    bool reanchor = false;
    int callbackBassError = 0;
    SfxCallbackEvent event;
    SfxArmContext context;
    context.source = SfxArmSource::Watchdog;
    {
        ScopedRealtimeBassErrorSink errorSink(&callbackBassError);
        QMutexLocker locker(&schedulerMutex_);
        if (!sfxSchedulerActive_) {
            // The GUI drainEvents fallback owns note sounds while the scheduler is off.
            sfxClockDivergenceStrikes_ = 0;
            return;
        }
        schedulerChartSecond = chartSecondForDecodePositionLocked(decodePosition);
        if (scheduledGroupSync_ != 0) {
            if (deferredMixerSyncHandle_.load(std::memory_order_acquire) != scheduledGroupSync_
                && scheduledSyncWasMissed(scheduledGroupTargetPosition_, decodePosition, graceBytes)) {
                // Our state says armed, the cursor is past the target by more than BASS's
                // dispatch block: the sync will never be delivered. The handle is still
                // registered with BASS and is removed after the lock is released.
                reason = "missed_sync";
                groupIndex = scheduledGroupIndex_;
                const ScheduledMixerAction action = scheduledMixerAction_;
                const quint32 deadHandle = scheduledGroupSync_;
                const QWORD targetPosition = scheduledGroupTargetPosition_;
                lateSeconds = mixerBytesToSeconds(decodePosition - targetPosition);
                scheduledGroupSync_ = 0;
                scheduledGroupIndex_ = -1;
                scheduledMixerAction_ = ScheduledMixerAction::None;
                scheduledGroupTargetPosition_ = 0;
                context.staleHandles[static_cast<std::size_t>(context.staleCount++)] = deadHandle;
                event.source = SfxArmSource::Watchdog;
                event.handle = deadHandle;
                event.decodePosition = decodePosition;
                event.targetPosition = targetPosition;
                event.groupIndex = groupIndex;
                event.groupSecond = groupIndex >= 0 && groupIndex < preparedGroups_.size()
                    ? preparedGroups_[groupIndex].second
                    : playbackSession_.backgroundTrackPendingStartSecond;
                if (lateGroupAction(lateSeconds) == LateGroupAction::TriggerNow) {
                    performScheduledActionLocked(groupIndex, action, &event);
                    event.kind = SfxCallbackEventKind::InlineTrigger;
                    sfxInlineTriggerCount_.fetch_add(1, std::memory_order_relaxed);
                } else {
                    if (action != ScheduledMixerAction::SfxGroup) {
                        performScheduledActionLocked(-1, ScheduledMixerAction::StartPendingBackgroundTrack, &event);
                    }
                    if (qIsFinite(schedulerChartSecond)) {
                        advanceCursorPastSecondLocked(schedulerChartSecond);
                    } else if (groupIndex >= 0 && playbackSession_.eventGroupIndex <= groupIndex) {
                        playbackSession_.eventGroupIndex = groupIndex + 1;
                    }
                    event.kind = SfxCallbackEventKind::InlineSkip;
                    sfxInlineSkipCount_.fetch_add(1, std::memory_order_relaxed);
                }
                armNextGroupSyncLocked(context);
                rearmed = scheduledGroupSync_ != 0;
            }
        } else {
            const int nextIndex = playbackSession_.eventGroupIndex;
            const bool hasGroup = nextIndex >= 0 && nextIndex < preparedGroups_.size();
            const bool hasPendingBackground = backgroundTrackSample_ != nullptr
                && playbackSession_.backgroundTrackPendingStart;
            if (hasGroup || hasPendingBackground) {
                // Active with nothing armed while work remains. No known path leaves the
                // scheduler here; if this row ever appears it names a state bug precisely.
                reason = "nothing_armed";
                groupIndex = nextIndex;
                armNextGroupSyncLocked(context);
                rearmed = scheduledGroupSync_ != 0;
            }
        }
        if (reason != nullptr) {
            event.armFailurePending = sfxSchedulerArmFailure_.pending;
            event.armFailureBassError = sfxSchedulerArmFailure_.bassError;
            event.armFailureTargetChartSecond = sfxSchedulerArmFailure_.targetChartSecond;
            sfxSchedulerArmFailure_ = SfxSchedulerArmFailure();
        }
        if (hasReference && qIsFinite(schedulerChartSecond)
            && chainClockDiverged(schedulerChartSecond, referenceChartSecond)) {
            if (++sfxClockDivergenceStrikes_ >= kChainClockDivergenceStrikes) {
                reanchor = true;
                sfxClockDivergenceStrikes_ = 0;
            }
        } else {
            sfxClockDivergenceStrikes_ = 0;
        }
    }
    if (event.kind != SfxCallbackEventKind::None || callbackBassError != 0 || event.armFailurePending) {
        event.callbackBassError = callbackBassError;
        logSfxCallbackEvent(event);
    }
    finishArmContext(context);
    if (reason != nullptr) {
        sfxWatchdogRecoveryCount_.fetch_add(1, std::memory_order_relaxed);
        appendAudioDebugLog(
            QString("bass_sfx_scheduler action=recover reason=%1 group_idx=%2 late_ms=%3 decode_pos=%4 sched_chart=%5 inline=%6 skipped=%7 rearmed=%8")
                .arg(QLatin1String(reason))
                .arg(groupIndex)
                .arg(lateSeconds * 1000.0, 0, 'f', 1)
                .arg(decodePosition)
                .arg(schedulerChartSecond, 0, 'f', 6)
                .arg(context.inlineTriggers)
                .arg(context.inlineSkips)
                .arg(rearmed ? 1 : 0));
    }
    if (reanchor) {
        // The decode<->chart mapping no longer agrees with the chart second the GUI is
        // rendering by a full second, over consecutive ticks. Nothing in the chain can
        // cause that; it is the signature of a mixer clock discontinuity outside it.
        // Re-anchoring here is what a manual pause/resume would have done.
        sfxWatchdogRecoveryCount_.fetch_add(1, std::memory_order_relaxed);
        appendAudioDebugLog(
            QString("bass_sfx_scheduler action=recover reason=clock_divergence sched_chart=%1 ref_chart=%2 delta_ms=%3 decode_pos=%4 anchor_chart=%5 anchor_decode_pos=%6")
                .arg(schedulerChartSecond, 0, 'f', 6)
                .arg(referenceChartSecond, 0, 'f', 6)
                .arg((schedulerChartSecond - referenceChartSecond) * 1000.0, 0, 'f', 1)
                .arg(decodePosition)
                .arg(sfxSchedulerAnchor_.chartSecond, 0, 'f', 6)
                .arg(sfxSchedulerAnchorDecodePosition_));
        resetCursor(referenceChartSecond, false);
    }
}

void BassPreviewAudioBackend::serviceSfxScheduler(double referenceChartSecond, bool hasReference)
{
    // PreviewAudioWorker never dispatches syncPreviewPlaybackClockTransaction(), so the
    // chain's worker-side upkeep has to ride on what it does execute: DrainEvents and
    // SyncBackgroundTrack every playback tick, plus its own health tick when the GUI stalls.
    drainSfxCallbackEvents();
    drainStaleSyncHandles();
    drainDeferredMixerSync();
    runSfxChainWatchdog(referenceChartSecond, hasReference);
}

void BassPreviewAudioBackend::drainSfxCallbackEvents()
{
    using namespace miacode::preview_audio::bass;
    SfxCallbackEvent event;
    for (std::size_t drained = 0;
         drained < SfxCallbackEventRing::kCapacity && sfxCallbackEventRing_.tryPop(&event);
         ++drained) {
        logSfxCallbackEvent(event);
    }
    const quint64 dropped = sfxCallbackEventRing_.takeDroppedCount();
    if (dropped > 0 && runtimeAudioDebugEnabled()) {
        appendAudioDebugLog(QString("bass_sfx_mixer_diag_drop count=%1").arg(dropped));
    }
}

void BassPreviewAudioBackend::logSfxCallbackEvent(
    const miacode::preview_audio::bass::SfxCallbackEvent& event) const
{
    using namespace miacode::preview_audio::bass;
    if (!runtimeAudioDebugEnabled()) {
        return;
    }
    const auto lateMicroseconds = [this](const SfxCallbackEvent& e) -> double {
        if (e.decodePosition == 0 && e.targetPosition == 0) {
            return 0.0;
        }
        return e.decodePosition >= e.targetPosition
            ? mixerBytesToSeconds(e.decodePosition - e.targetPosition) * 1e6
            : -mixerBytesToSeconds(e.targetPosition - e.decodePosition) * 1e6;
    };
    const auto playedList = [](const SfxCallbackEvent& e) {
        QString playedKinds;
        for (std::size_t index = 0; index < kPlayedSfxKindCount; ++index) {
            if ((e.played.mask & (quint32(1) << static_cast<quint32>(index))) == 0) {
                continue;
            }
            if (!playedKinds.isEmpty()) {
                playedKinds.append(QLatin1Char(','));
            }
            playedKinds.append(QStringLiteral("%1:%2")
                .arg(QLatin1String(playedSfxKindName(index)))
                .arg(static_cast<double>(e.played.gains[index]), 0, 'f', 2));
        }
        return playedKinds.isEmpty() ? QStringLiteral("(none)") : playedKinds;
    };
    if (event.kind == SfxCallbackEventKind::Deferred) {
        appendAudioDebugLog(
            QString("bass_sfx_mixer_deferred reason=scheduler_busy handle=%1 pending=%2")
                .arg(event.handle)
                .arg(event.expectedHandle));
        return;
    }
    if (event.kind == SfxCallbackEventKind::Drop) {
        const char* reason = event.dropReason == SfxCallbackDropReason::Inactive
            ? "inactive"
            : "stale_handle";
        appendAudioDebugLog(
            QString("bass_sfx_mixer_drop reason=%1 handle=%2 expected=%3 deferred=%4")
                .arg(QLatin1String(reason))
                .arg(event.handle)
                .arg(event.expectedHandle)
                .arg(event.processedAfterContention ? 1 : 0));
    } else if (event.kind == SfxCallbackEventKind::Trigger
               || event.kind == SfxCallbackEventKind::InlineTrigger) {
        // One row name for every group the chain played, whichever path delivered it:
        // `inline=1 source=…` marks a group played by an arm pass because BASS could not
        // have fired its sync; `late_us` is the cursor's distance past the armed target
        // when the sound started (0 for a callback-delivered sync).
        appendAudioDebugLog(
            QString("bass_sfx_mixer_trigger group_idx=%1 group_second=%2 count=%3 started_bgm=%4 played=%5 deferred=%6 late_us=%7 inline=%8 source=%9")
                .arg(event.groupIndex)
                .arg(event.groupSecond, 0, 'f', 6)
                .arg(event.triggeredCount)
                .arg(event.startedBackground ? 1 : 0)
                .arg(playedList(event))
                .arg(event.processedAfterContention ? 1 : 0)
                .arg(lateMicroseconds(event), 0, 'f', 0)
                .arg(event.kind == SfxCallbackEventKind::InlineTrigger ? 1 : 0)
                .arg(QLatin1String(sfxArmSourceName(event.source))));
    } else if (event.kind == SfxCallbackEventKind::InlineSkip) {
        appendAudioDebugLog(
            QString("bass_sfx_scheduler action=skip reason=sync_dead_on_arm group_idx=%1 group_second=%2 late_ms=%3 source=%4 started_bgm=%5")
                .arg(event.groupIndex)
                .arg(event.groupSecond, 0, 'f', 6)
                .arg(lateMicroseconds(event) / 1000.0, 0, 'f', 1)
                .arg(QLatin1String(sfxArmSourceName(event.source)))
                .arg(event.startedBackground ? 1 : 0));
    }

    if (event.touchholdChanged) {
        TouchholdTransition transition;
        transition.changed = true;
        transition.owner = event.touchholdOwner;
        transition.previousOwner = event.touchholdPreviousOwner;
        transition.second = event.touchholdSecond;
        transition.spanStartSecond = event.touchholdSpanStartSecond;
        logTouchholdTransition(transition);
    }
    if (event.callbackBassError != 0) {
        noteBassErrCode("sfx_scheduler/mixer_callback", event.callbackBassError);
    }
    if (event.armFailurePending) {
        SfxSchedulerArmFailure failure;
        failure.pending = true;
        failure.bassError = event.armFailureBassError;
        failure.targetChartSecond = event.armFailureTargetChartSecond;
        logSfxSchedulerArmFailure(failure);
    }
}

void BassPreviewAudioBackend::reconcileTouchholdVoice(double second, TouchholdTransition* out)
{
    if (touchholdSample_ == nullptr) {
        return;
    }
    const int owner = miacode::preview_sfx_timeline::touchholdOwnerSpanIndexAt(
        preparedTimeline_.touchholdSpans, second);
    if (owner == touchholdOwnerSpanIndex_) {
        return;  // voice already belongs to the right span — leave it playing
    }
    const int previousOwner = touchholdOwnerSpanIndex_;
    touchholdOwnerSpanIndex_ = owner;

    TouchholdTransition transition;
    transition.changed = true;
    transition.owner = owner;
    transition.previousOwner = previousOwner;
    transition.second = second;
    if (owner < 0) {
        touchholdSample_->stop();
    } else {
        const TouchholdSpan& span = preparedTimeline_.touchholdSpans[owner];
        touchholdSample_->setCurrentSec(qMax(0.0, second - span.startSecond));
        touchholdSample_->play();
        transition.spanStartSecond = span.startSecond;
    }
    // Recorded for the caller when it holds schedulerMutex_, logged inline when it does
    // not. The audio-thread route arrives here from triggerGroup() with that lock held, and
    // this used to write the file underneath it -- the same defect as logPlaybackStatus,
    // on the worse thread, and the one instance the branch audit's T-1 missed.
    if (out != nullptr) {
        *out = transition;
        return;
    }
    logTouchholdTransition(transition);
}

void BassPreviewAudioBackend::logTouchholdTransition(const TouchholdTransition& transition) const
{
    if (!transition.changed) {
        return;
    }
    if (transition.owner < 0) {
        appendAudioDebugLog(
            QString("bass_sfx_touchhold action=stop prev_owner=%1 second=%2")
                .arg(transition.previousOwner)
                .arg(transition.second, 0, 'f', 6));
        return;
    }
    // The third sound source with no log of its own. Only fires on an ownership
    // change (reconcileTouchholdVoice returns early when the voice already belongs to
    // the right span), so this stays rare even during dense touch-hold sections.
    appendAudioDebugLog(
        QString("bass_sfx_touchhold action=start owner=%1 prev_owner=%2 second=%3 span_start=%4")
            .arg(transition.owner)
            .arg(transition.previousOwner)
            .arg(transition.second, 0, 'f', 6)
            .arg(transition.spanStartSecond, 0, 'f', 6));
}

void BassPreviewAudioBackend::pauseTouchholdVoices()
{
    MC_OP("BassPreviewAudioBackend::pauseTouchholdVoices");
    if (touchholdSample_ != nullptr) {
        touchholdSample_->stop();
    }
    touchholdOwnerSpanIndex_ = -1;
}

void BassPreviewAudioBackend::restoreTouchholdVoices(double second)
{
    MC_OP("BassPreviewAudioBackend::restoreTouchholdVoices");
    pauseTouchholdVoices();
    reconcileTouchholdVoice(second);
}


bool BassPreviewAudioBackend::playKindInternal(
    const QString& kind,
    double gain,
    int* nativeErrorCode)
{
    if (nativeErrorCode != nullptr) {
        *nativeErrorCode = 0;
    }
    Sample* sample = sampleForKind(kind);
    if (sample == nullptr) {
        return false;
    }
    return sample->playOneShot(gain, nativeErrorCode);
}

bool BassPreviewAudioBackend::audition(const QString& kind, double gain)
{
    MC_OP("BassPreviewAudioBackend::audition");
    lastNativeErrorCode_ = 0;
    if (!initializeAudioEngine() || masterMixer_ == 0) {
        return false;
    }
    if (!playbackSession_.masterRunning) {
        resetMasterMixerClock(0.0);
        // G1 Commit 6: master mixer was started at engine init and never stops.
        playbackSession_.masterRunning = true;
        audioHealthPlaybackRunning_.store(true, std::memory_order_release);
    }
    const bool started = playKindInternal(kind, gain, &lastNativeErrorCode_);
    // This path emits a real note sound while bypassing the scheduler, the group
    // cursor, and therefore both group-level logs. Unlogged, an audition was
    // indistinguishable from "no sound was played at all" in a capture — which is
    // precisely the ambiguity that stalled the device-change investigation.
    appendAudioDebugLog(
        QString("bass_sfx_audition kind=%1 gain=%2 started=%3")
            .arg(kind)
            .arg(gain, 0, 'f', 2)
            .arg(started ? 1 : 0));
    return started;
}

void BassPreviewAudioBackend::stopAll()
{
    MC_OP("BassPreviewAudioBackend::stopAll");
    stopPlaybackSession();
    preparedPlayback_ = PreparedPlaybackState();
    retainedPlaybackMode_ = RetainedPlaybackMode::None;
}

void BassPreviewAudioBackend::prepareForShutdown()
{
    MC_OP("BassPreviewAudioBackend::prepareForShutdown");
    shuttingDown_.store(true, std::memory_order_release);
    miacode::preview_audio::PreviewBassEmergencyPause::disarm();
    stopAll();
}
