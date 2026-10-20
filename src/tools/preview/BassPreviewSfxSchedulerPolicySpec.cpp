#include <QString>
#include <QTextStream>

#include <limits>

#include "audio/bass/BassPreviewMasterMixerPolicy.h"
#include "audio/bass/BassPreviewSfxCallbackRing.h"
#include "audio/bass/BassPreviewSfxSchedulerPolicy.h"

namespace {

bool require(bool condition, const QString& message, QTextStream& err)
{
    if (!condition) {
        err << "FAIL: " << message << Qt::endl;
    }
    return condition;
}

}  // namespace

int main()
{
    using miacode::preview_audio::bass::SfxSchedulerAnchor;
    using miacode::preview_audio::bass::chartSecondForMixerSecond;
    using miacode::preview_audio::bass::masterMixerPolicyFromOverrides;
    using miacode::preview_audio::bass::mixerSecondForChartSecond;
    using miacode::preview_audio::bass::shouldLogDisarm;

    QTextStream err(stderr);
    QTextStream out(stdout);
    bool ok = true;

    ok &= require(
        shouldLogDisarm(true, false, -1),
        QStringLiteral("an active scheduler disarm remains visible"), err);
    ok &= require(
        shouldLogDisarm(false, true, -1),
        QStringLiteral("a disarm with a sync remains visible"), err);
    ok &= require(
        shouldLogDisarm(false, false, 0),
        QStringLiteral("a disarm with a group remains visible"), err);
    ok &= require(
        !shouldLogDisarm(false, false, -1),
        QStringLiteral("only a proven no-op disarm is suppressed"), err);

    const SfxSchedulerAnchor oneX {10.0, 120.0, 1.0};
    ok &= require(
        mixerSecondForChartSecond(oneX, 12.5) == 122.5,
        QStringLiteral("one-times chart seconds advance the master mixer equally"), err);

    const SfxSchedulerAnchor halfX {10.0, 120.0, 0.5};
    ok &= require(
        mixerSecondForChartSecond(halfX, 12.5) == 125.0,
        QStringLiteral("half-speed chart seconds map through the active playback rate"), err);

    const SfxSchedulerAnchor invalidRate {10.0, 120.0, 0.0};
    ok &= require(
        mixerSecondForChartSecond(invalidRate, 12.5) == 122.5,
        QStringLiteral("an invalid rate falls back to one-times scheduling"), err);

    const SfxSchedulerAnchor buffered {10.0, 120.0, 1.0, 0.030};
    ok &= require(
        qAbs(mixerSecondForChartSecond(buffered, 12.5) - 122.47) < 1e-9,
        QStringLiteral("an output buffer advances the mixer sync by its audible lead"), err);
    ok &= require(
        chartSecondForMixerSecond(buffered, 122.5) == 12.5,
        QStringLiteral("output buffering does not alter decode-cursor clock conversion"), err);

    const SfxSchedulerAnchor rebuildAnchor {5.0, 100.0, 1.0};
    ok &= require(
        chartSecondForMixerSecond(rebuildAnchor, 150.0) == 55.0,
        QStringLiteral("a settings rebuild reanchors at the current master position, not the last SFX"), err);

    const auto defaultPolicy = masterMixerPolicyFromOverrides(QString(), QString());
    ok &= require(
        defaultPolicy.bufferMs >= 0.0 && defaultPolicy.threadCount > 0,
        QStringLiteral("the master mixer exposes a valid default configuration"), err);
    const auto legacyPolicy = masterMixerPolicyFromOverrides(
        QStringLiteral("0"), QStringLiteral("8"));
    ok &= require(
        legacyPolicy.bufferMs == 0.0 && legacyPolicy.threadCount == 8
            && legacyPolicy.bufferOverrideValid && legacyPolicy.threadOverrideValid,
        QStringLiteral("the former zero-buffer eight-thread setup remains available for A/B"), err);
    const auto invalidPolicy = masterMixerPolicyFromOverrides(
        QStringLiteral("nan"), QStringLiteral("17"));
    ok &= require(
        invalidPolicy.bufferMs == defaultPolicy.bufferMs
            && invalidPolicy.threadCount == defaultPolicy.threadCount
            && !invalidPolicy.bufferOverrideValid && !invalidPolicy.threadOverrideValid,
        QStringLiteral("invalid overrides fall back to safe defaults"), err);

    using miacode::preview_audio::bass::SfxCallbackEvent;
    using miacode::preview_audio::bass::SfxCallbackEventKind;
    using miacode::preview_audio::bass::SfxCallbackEventRing;
    using miacode::preview_audio::bass::PlayedSfxSnapshot;
    PlayedSfxSnapshot played;
    played.record(QStringLiteral("judge_break"), 0.75);
    ok &= require(
        played.mask != 0,
        QStringLiteral("callback diagnostics encode a played kind without storing QString"), err);
    SfxCallbackEventRing ring;
    for (std::size_t index = 0; index + 1 < SfxCallbackEventRing::kCapacity; ++index) {
        SfxCallbackEvent event;
        event.kind = SfxCallbackEventKind::Trigger;
        event.handle = static_cast<quint32>(index + 1);
        ok &= require(ring.tryPush(event), QStringLiteral("callback event ring accepts its usable capacity"), err);
    }
    SfxCallbackEvent overflow;
    ok &= require(
        !ring.tryPush(overflow) && ring.takeDroppedCount() == 1,
        QStringLiteral("callback event ring reports overflow without blocking"), err);
    for (std::size_t index = 0; index + 1 < SfxCallbackEventRing::kCapacity; ++index) {
        SfxCallbackEvent event;
        ok &= require(
            ring.tryPop(&event) && event.handle == static_cast<quint32>(index + 1),
            QStringLiteral("callback event ring preserves FIFO order"), err);
    }
    SfxCallbackEvent empty;
    ok &= require(
        !ring.tryPop(&empty),
        QStringLiteral("callback event ring is empty after a complete drain"), err);

    using miacode::preview_audio::bass::kDeferredSyncMaxLateSeconds;
    using miacode::preview_audio::bass::kMissedSyncGraceSeconds;
    using miacode::preview_audio::bass::scheduledSyncWasMissed;
    using miacode::preview_audio::bass::shouldReplayDeferredSync;
    ok &= require(
        !scheduledSyncWasMissed(1000, 999, 100),
        QStringLiteral("a sync ahead of the decode cursor is still pending"), err);
    ok &= require(
        !scheduledSyncWasMissed(1000, 1099, 100),
        QStringLiteral("a sync the cursor just crossed is still being dispatched"), err);
    ok &= require(
        scheduledSyncWasMissed(1000, 1100, 100) && scheduledSyncWasMissed(1000, 50000, 100),
        QStringLiteral("a sync the cursor is past by the grace can no longer fire"), err);
    ok &= require(
        shouldReplayDeferredSync(0.0) && shouldReplayDeferredSync(-0.004)
            && shouldReplayDeferredSync(kDeferredSyncMaxLateSeconds),
        QStringLiteral("a deferred sync near its note time is replayed"), err);
    ok &= require(
        !shouldReplayDeferredSync(kDeferredSyncMaxLateSeconds + 0.001)
            && !shouldReplayDeferredSync(std::numeric_limits<double>::quiet_NaN()),
        QStringLiteral("a deferred sync from a stalled worker is skipped"), err);

    // Measured against the bundled BASS 2.4.18 / BASSmix 2.4.13 with the production master
    // configuration (float stereo, NONSTOP|POSEX, zero buffer, four mixer threads):
    //   - a POS|MIXTIME|ONETIME sync armed exactly AT the current decode position never
    //     fires; one armed one sample ahead fires at exactly that sample;
    //   - a sync armed behind the cursor never fires;
    //   - a sync armed from inside a sync callback fires even when its position lies in the
    //     block currently being mixed, and chains survive 40 ms callback stalls.
    // So the only way the chain dies is arming at or behind the cursor, and the check that
    // decides to fire such a group inline must treat equality as dead.
    using miacode::preview_audio::bass::armedSyncCannotFire;
    ok &= require(
        armedSyncCannotFire(1000, 1000) && armedSyncCannotFire(1000, 1008),
        QStringLiteral("a sync armed at or behind the decode cursor can never fire"), err);
    ok &= require(
        !armedSyncCannotFire(1008, 1000),
        QStringLiteral("a sync armed one sample ahead of the decode cursor is live"), err);

    // The missed-sync grace only has to cover the one mix block in which BASS may still be
    // dispatching a correctly armed sync (10 ms on every measured device); a 200 ms grace
    // was a fifth of a second of silence per miss and, because the recovery re-anchored and
    // skipped, it also dropped every group in that window.
    ok &= require(
        kMissedSyncGraceSeconds <= 0.050 && kMissedSyncGraceSeconds >= 0.010,
        QStringLiteral("the missed-sync grace is a few mix blocks, not a fifth of a second"), err);

    // A group the chain is late for is still played when it is close to its note time; only
    // a group that waited out a real stall is skipped, so a recovery never releases a burst
    // of stale note sounds and never goes silent for a group it could still have played.
    using miacode::preview_audio::bass::LateGroupAction;
    using miacode::preview_audio::bass::kLateGroupCatchUpSeconds;
    using miacode::preview_audio::bass::lateGroupAction;
    ok &= require(
        lateGroupAction(0.0) == LateGroupAction::TriggerNow
            && lateGroupAction(kMissedSyncGraceSeconds) == LateGroupAction::TriggerNow
            && lateGroupAction(kLateGroupCatchUpSeconds) == LateGroupAction::TriggerNow,
        QStringLiteral("a group within the catch-up window is triggered late rather than lost"), err);
    ok &= require(
        lateGroupAction(kLateGroupCatchUpSeconds + 0.001) == LateGroupAction::Skip
            && lateGroupAction(std::numeric_limits<double>::quiet_NaN()) == LateGroupAction::Skip,
        QStringLiteral("a group past the catch-up window is skipped"), err);
    ok &= require(
        kDeferredSyncMaxLateSeconds == kLateGroupCatchUpSeconds,
        QStringLiteral("deferred replay and inline catch-up share one lateness window"), err);

    // The scheduler's own clock (anchor + master decode cursor) is compared with the chart
    // second the GUI passes on every tick. A divergence beyond one second, persisting over
    // consecutive ticks, means the decode<->chart mapping is broken by something the chain
    // cannot see (a device re-initialisation, a position discontinuity) and the session is
    // re-anchored to the reference instead of staying silent until a manual pause.
    using miacode::preview_audio::bass::chainClockDiverged;
    using miacode::preview_audio::bass::kChainClockDivergenceSeconds;
    using miacode::preview_audio::bass::kChainClockDivergenceStrikes;
    ok &= require(
        !chainClockDiverged(10.0, 10.2) && !chainClockDiverged(10.0, 9.5),
        QStringLiteral("ordinary stall drift between the mixer clock and the wall clock is tolerated"), err);
    ok &= require(
        chainClockDiverged(10.0, 10.0 + kChainClockDivergenceSeconds + 0.01)
            && chainClockDiverged(10.0 + kChainClockDivergenceSeconds + 0.01, 10.0),
        QStringLiteral("a divergence beyond the threshold in either direction is flagged"), err);
    ok &= require(
        !chainClockDiverged(std::numeric_limits<double>::quiet_NaN(), 10.0)
            && !chainClockDiverged(10.0, std::numeric_limits<double>::infinity()),
        QStringLiteral("a non-finite clock never triggers a re-anchor"), err);
    ok &= require(
        kChainClockDivergenceStrikes >= 2,
        QStringLiteral("a single divergent tick is never acted on"), err);

    if (ok) {
        out << "BASS preview SFX scheduler policy spec passed." << Qt::endl;
    }
    return ok ? 0 : 1;
}
