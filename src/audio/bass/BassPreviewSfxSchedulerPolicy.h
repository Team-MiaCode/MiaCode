#pragma once

#include <cmath>
#include <cstdint>

#include "audio/bass/BassPreviewMasterMixerPolicy.h"

namespace miacode::preview_audio::bass {

inline bool shouldLogDisarm(bool wasActive, bool hadSync, int groupIndex) noexcept
{
    return wasActive || hadSync || groupIndex != -1;
}

// The master mixer is intentionally never stopped.  Each live preview session
// therefore records the mixer position that corresponds to its chart-second
// anchor, allowing SFX BASS_SYNC_POS positions to remain meaningful across
// separate play / pause / seek cycles.
struct SfxSchedulerAnchor {
    double chartSecond = 0.0;
    double mixerSecond = 0.0;
    double playbackRate = 1.0;
    double outputBufferSeconds = 0.0;
};

inline double mixerSecondForChartSecond(const SfxSchedulerAnchor& anchor, double chartSecond)
{
    const double rate = std::isfinite(anchor.playbackRate) && anchor.playbackRate > 0.0
        ? anchor.playbackRate
        : 1.0;
    return anchor.mixerSecond + (chartSecond - anchor.chartSecond) / rate
        - validOutputBufferSeconds(anchor.outputBufferSeconds);
}

inline double chartSecondForMixerSecond(const SfxSchedulerAnchor& anchor, double mixerSecond)
{
    const double rate = std::isfinite(anchor.playbackRate) && anchor.playbackRate > 0.0
        ? anchor.playbackRate
        : 1.0;
    return anchor.chartSecond + (mixerSecond - anchor.mixerSecond) * rate;
}

// The scheduler keeps exactly one BASS_SYNC_POS armed and arms the next group from its
// callback, so a sync that never fires silences every later note sound until something
// re-arms. Measured against the bundled BASS (2.4.18 / BASSmix 2.4.13) on the production
// master configuration: a position sync armed AT or BEHIND the decode cursor is never
// delivered, one armed a single sample ahead fires at exactly that sample, and a sync
// armed from inside a sync callback fires even when it lies in the block being mixed.
// The chain therefore only dies by arming at or behind the cursor -- the anchor's position
// read racing the mixer thread, a group whose lead rounds to zero samples, or a group
// armed after a late (deferred / watchdog) replay -- and every arm site checks for that.
inline bool armedSyncCannotFire(
    std::uint64_t targetPosition,
    std::uint64_t decodePosition) noexcept
{
    return decodePosition >= targetPosition;
}

// Worker-side watchdog margin. A correctly armed sync fires while BASS mixes the block that
// contains its target, and the cursor another thread reads advances by whole blocks, so a
// cursor that is past the target by more than a couple of blocks while our state still
// says "armed" can only mean the sync was never delivered. Kept short: every millisecond
// here is a millisecond of late note sound when the watchdog has to step in.
inline constexpr double kMissedSyncGraceSeconds = 0.020;

inline bool scheduledSyncWasMissed(
    std::uint64_t targetPosition,
    std::uint64_t decodePosition,
    std::uint64_t graceBytes) noexcept
{
    return decodePosition >= targetPosition && decodePosition - targetPosition >= graceBytes;
}

// A group the chain is late for -- a deferred sync the worker replays, a dead sync the
// watchdog catches, a group whose arm was found to be behind the cursor -- is still
// played when it is close to its note time. Only a group that waited out a real stall is
// skipped, so a recovery never releases a burst of stale note sounds and never goes
// silent for a group it could still have played.
inline constexpr double kLateGroupCatchUpSeconds = 0.150;
inline constexpr double kDeferredSyncMaxLateSeconds = kLateGroupCatchUpSeconds;

enum class LateGroupAction {
    TriggerNow,
    Skip,
};

inline LateGroupAction lateGroupAction(double lateSeconds) noexcept
{
    return std::isfinite(lateSeconds) && lateSeconds <= kLateGroupCatchUpSeconds
        ? LateGroupAction::TriggerNow
        : LateGroupAction::Skip;
}

inline bool shouldReplayDeferredSync(double lateSeconds) noexcept
{
    return lateGroupAction(lateSeconds) == LateGroupAction::TriggerNow;
}

// Upper bound on the groups one arm pass may trigger inline before it leaves the rest to
// the next worker tick; keeps a pathological cursor jump from replaying a whole chart
// from inside one mixer callback.
inline constexpr int kMaxInlineCatchUpGroups = 8;

// Last-resort net for a broken decode<->chart mapping. The scheduler's own chart second
// (anchor + master decode cursor) is compared with the chart second the GUI passes on
// every tick; ordinary master-mixer stall drift is on the order of 100 ms and must stay
// untouched (the BGM shares that drift and is what the user hears), so only a divergence
// of a full second, persisting over consecutive ticks, re-anchors to the reference. Every
// firing is logged as `reason=clock_divergence`; it is a signal that something outside
// the chain (a device re-initialisation, a position discontinuity) moved the mixer clock.
inline constexpr double kChainClockDivergenceSeconds = 1.0;
inline constexpr int kChainClockDivergenceStrikes = 3;

inline bool chainClockDiverged(
    double schedulerChartSecond,
    double referenceChartSecond,
    double thresholdSeconds = kChainClockDivergenceSeconds) noexcept
{
    if (!std::isfinite(schedulerChartSecond) || !std::isfinite(referenceChartSecond)) {
        return false;
    }
    return std::fabs(schedulerChartSecond - referenceChartSecond) > thresholdSeconds;
}

}  // namespace miacode::preview_audio::bass
