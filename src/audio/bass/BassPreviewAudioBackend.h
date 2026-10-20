#pragma once

#include <array>
#include <atomic>
#include <memory>

#include <QObject>
#include <QHash>
#include <QMutex>

#include "audio/PreviewAudioMixConfig.h"
#include "audio/bass/BassPreviewDebugLogRouting.h"
#include "audio/bass/BassPreviewSfxCallbackRing.h"
#include "audio/bass/BassPreviewOutputGlitchProbeState.h"
#include "audio/bass/BassPreviewSfxSchedulerPolicy.h"
#include "audio/bass/PreviewBassDeviceLease.h"
#include "audio/PreviewAudioBackend.h"
#include "audio/PreviewAudioHealth.h"
#include "audio/PreviewAudioWorkerFactory.h"

class BassPreviewAudioBackend final : public QObject, public miacode::preview_audio::PreviewAudioBackend
{
public:
    using PausePreviewResult = miacode::preview_audio::PausePreviewResult;
    using RetainedPlaybackMode = miacode::preview_audio::RetainedPlaybackMode;
    using RetainedBgmState = miacode::preview_audio::RetainedBgmState;
    using Event = miacode::preview_sfx_timeline::Event;
    using TouchholdSpan = miacode::preview_sfx_timeline::TouchholdSpan;
    using CollapsedEventGroup = miacode::preview_sfx_timeline::CollapsedEventGroup;

    explicit BassPreviewAudioBackend(QObject* parent = nullptr);
    ~BassPreviewAudioBackend() override;

    QString backendId() const override;
    bool canBePrimary(QString* reason = nullptr) const override;
    int nativeErrorCode() const noexcept override;
    void clearNativeErrorCode() noexcept override;

    void setWarmupResolvedPaths(const QString& chartPath, const QString& trackPath, const QString& sfxDir) override;
    void reloadAssets(const PreviewAudioSettings& settings) override;
    bool audioEngineInitialized() const override;
    void setChartPath(const QString& chartPath) override;
    void setBackgroundTrackOffsetSeconds(double seconds) override;
    void setBackgroundTrackPlaybackRate(double rate) override;
    void applyPlaybackRateAtChartSecond(double rate, double chartSecond) override;
    void applyLevels(const PreviewAudioSettings& settings) override;
    void configureTimeline(
        const QVector<TimelineNoteMarker>& noteMarkers,
        double playbackRate,
        const PreviewTimingSettings& timingSettings) override;
    void clearTimeline() override;
    void setPlaybackTransactionId(quint64 transactionId) override;
    double preparePreviewPlaybackTransaction(double startSecond, bool resumeFromPause, double playbackRate) override;
    void commitPreparedPreviewPlayback() override;
    void cancelPreparedPreviewPlayback() override;
    double preparedStartSecond() const override;
    void applyPausedPreviewState(
        const QVector<TimelineNoteMarker>& noteMarkers,
        bool noteMarkersChanged,
        double pauseSecond,
        double playbackRate,
        const PreviewTimingSettings& timingSettings) override;
    double startPreviewPlaybackTransaction(double startSecond, bool resumeFromPause, double playbackRate) override;
    PausePreviewResult capturePausedPreviewTransaction() override;
    PausePreviewResult pausePreviewPlaybackTransaction() override;
    double resumeRetainedPreviewPlaybackTransaction() override;
    double seekRetainedPreviewPlaybackTransaction(double targetSecond, bool continuePlaying) override;
    void resetRetainedPreviewPlaybackTransaction(double targetSecond) override;
    void clearRetainedPreviewPlaybackTransaction() override;
    RetainedPlaybackMode retainedPlaybackMode() const override;
    RetainedBgmState retainedBgmState() const override;
    double authoritativePlaybackSecond() const override;
    void stopSfxVoices() override;
    void invalidateOutputDevice() override;
    double syncPreviewPlaybackClockTransaction(double fallbackSecond) override;
    void resetCursor(double second, bool includeCurrentSecond) override;
    void drainEvents(double second) override;
    void pauseTouchholdVoices() override;
    void restoreTouchholdVoices(double second) override;
    void syncBackgroundTrack(double timelineSecond) override;
    bool hasBackgroundTrack() const override;
    bool isBackgroundTrackRunning() const override;
    void startBackgroundTrack(double second) override;
    void seekBackgroundTrack(double second) override;
    void pauseBackgroundTrack() override;
    double backgroundPlaybackSecond() const override;
    bool audition(const QString& kind, double gain = 1.0) override;
    void stopAll() override;
    void prepareForShutdown() override;
    miacode::preview_audio::PreviewAudioHealthSample sampleHealth() override;

private:
    struct Sample;

    enum class ScheduledMixerAction {
        None,
        SfxGroup,
        StartPendingBackgroundTrack,
        SfxGroupAndStartPendingBackgroundTrack,
    };

    struct PreparedAssetState {
        QString chartPath;
        QString trackPath;
        QString sfxDir;
        // Content stamp (size:mtime) of the resolved track, so setChartPath can
        // detect a same-path/new-content track and force a reload instead of
        // skipping on path equality alone.
        QString trackStamp;
    };

    struct TimelineProgramState {
        QVector<Event> events;
        QVector<TouchholdSpan> touchholdSpans;
        QVector<TimelineNoteMarker> sourceNoteMarkers;
    };

    struct PreparedPlaybackState {
        bool pending = false;
        bool resumeFromPause = false;
        double startSecond = 0.0;
    };

    struct PlaybackSessionState {
        int eventGroupIndex = 0;
        bool masterRunning = false;
        bool backgroundTrackRunning = false;
        bool backgroundTrackPendingStart = false;
        bool backgroundTrackPastEnd = false;
        double backgroundTrackPendingStartSecond = 0.0;
        double backgroundTrackOffsetSeconds = 0.0;
        double backgroundTrackPlaybackRate = 1.0;
        double sessionStartSecond = 0.0;
        double sessionPlaybackRate = 1.0;
        double lastAuthoritativeSecond = 0.0;
        // Chart second of the most recent GUI tick (drainEvents / syncBackgroundTrack) while
        // the transport was running; -1 outside a live session. The only live clock the
        // backend receives from outside once playback is under way.
        double lastTickSecond = -1.0;
        double lastStatusLogSecond = -1.0;
        // Underrun / buffer-health probe state. Separate from lastStatusLogSecond so the
        // (much coarser) health cadence and the ~1 Hz bass_status cadence stay independent.
        double lastTriggeredGroupSecond = -1.0;
        int lastTriggeredGroupIndex = -1;
        int triggeredGroupCount = 0;
    };

    // Deferred report of armNextGroupSyncLocked's self-deactivation. That function runs
    // with schedulerMutex_ held and, through handleMixerGroupSync, on the BASS mixer
    // thread, so it records the failure here instead of logging it; the callers drain
    // this once they have released the lock.
    struct SfxSchedulerArmFailure {
        bool pending = false;
        int bassError = 0;
        double targetChartSecond = 0.0;
    };

    QString resolveTrackPath(const QString& chartPath) const;
    QString resolveSfxDir() const;
    bool runtimeLibrariesPresent() const;
    bool initializeAudioEngine();
    bool ensureBassFxLoaded();
    void unloadBassFx();
    void loadOptionalPlugins();
    void unloadOptionalPlugins();
    void initializeAssets();
    void resetAssets();
    void clearPreparedTimeline();
    void rebuildPreparedTimeline(
        const QVector<TimelineNoteMarker>& noteMarkers,
        double playbackRate,
        const PreviewTimingSettings& timingSettings);
    void rebuildPreparedGroups();
    void refreshPreparedAssets();
    void applySampleLevels();
    Sample* sampleForKind(const QString& kind) const;
    double retainedTransportSecond() const;
    bool retainedSecondMatches(double targetSecond) const;
    void noteInitWindowOpened(const QString& reason);
    void noteTransportReady(const QString& reason);
    void appendBassDebugLog(
        miacode::preview_audio::bass::BassDebugOperation operation,
        const QString& payload = QString(),
        bool initWindowContext = false) const;
    void setBackgroundTrackSampleSpeed(double rate);
    void invalidateRetainedPlaybackState(const QString& reason);
    void updateRetainedBgmState();
    void logTrackFileMissingAfterLoadIfNeeded();
    void suspendPlaybackTransport();
    void anchorTransportToSecond(double targetSecond, const QString& reason);
    void clearResidualVoicesForPausedReposition();
    void repositionMasterTransportClock(double targetSecond);
    void repositionPausedTransportToSecond(double targetSecond, const QString& reason);
    void startTransportFromCurrentAnchor();
    void resetMasterMixerClock(double startSecond);
    // `reason` names the caller in the emitted `action=disarm` line. A disarm that is
    // never followed by a re-anchor silently drops live SFX to the GUI drainEvents
    // fallback, and the log only showed anchors, so the author of a missing re-anchor
    // could not be identified from a capture.
    void disarmSfxScheduler(const char* reason);
    void anchorSfxScheduler(double chartSecond);
    double currentSfxSchedulerChartSecond(double fallbackSecond) const;
    // Chart second for a master decode position under the current anchor; only meaningful
    // while sfxSchedulerActive_. Must be called with schedulerMutex_ held.
    double chartSecondForDecodePositionLocked(quint64 decodePosition) const;
    // Best available live chart second for re-anchoring: the scheduler's own clock while it
    // is active, else the last chart second the GUI ticked while the transport ran, else the
    // transport snapshot. lastAuthoritativeSecond alone is the session start for the whole
    // of a live session (the worker never dispatches syncPreviewPlaybackClockTransaction),
    // so using it directly would rewind the SFX cursor to the start after an inactive
    // scheduler met a settings change.
    double liveChartSecondEstimate() const;
    // Moves only the event-group cursor past `second`; leaves the anchor and any armed sync
    // alone. Must be called with schedulerMutex_ held.
    void advanceCursorPastSecondLocked(double second);

    // Everything one arm pass produces besides the armed sync itself. Both threads that arm
    // (the worker and the BASS mixer callback) fill this on their own stack and hand the
    // contents to the log / to BASS_ChannelRemoveSync only once schedulerMutex_ is gone.
    struct SfxArmContext {
        miacode::preview_audio::bass::SfxArmSource source =
            miacode::preview_audio::bass::SfxArmSource::Callback;
        // Dead syncs (armed at or behind the cursor) that still sit in BASS's sync list.
        std::array<quint32, miacode::preview_audio::bass::kMaxInlineCatchUpGroups> staleHandles{};
        int staleCount = 0;
        // Groups played (or skipped) inline because their sync could never have fired.
        std::array<miacode::preview_audio::bass::SfxCallbackEvent,
            miacode::preview_audio::bass::kMaxInlineCatchUpGroups> events{};
        int eventCount = 0;
        int inlineTriggers = 0;
        int inlineSkips = 0;
    };
    // Arms the next group / pending-BGM sync and verifies it against the live decode cursor:
    // a target the cursor has already reached can never fire (see
    // BassPreviewSfxSchedulerPolicy.h), so that group is played inline and the pass moves on
    // to the following group, bounded by kMaxInlineCatchUpGroups. Must be called with
    // schedulerMutex_ held.
    void armNextGroupSyncLocked(SfxArmContext& context);
    // The action a fired (or dead) sync stands for: start the pending BGM and/or play the
    // group, advance the cursor, and describe it in `event`. Must be called with
    // schedulerMutex_ held.
    void performScheduledActionLocked(
        int groupIndex,
        ScheduledMixerAction action,
        miacode::preview_audio::bass::SfxCallbackEvent* event);
    void processMixerGroupSyncLocked(
        quint32 handle,
        bool processedAfterContention,
        miacode::preview_audio::bass::SfxCallbackEvent* event,
        SfxArmContext& context);
    // Worker side of an arm pass: removes dead syncs and logs inline events. Must be called
    // with schedulerMutex_ released.
    void finishArmContext(const SfxArmContext& context);
    // Mixer-callback side: dead syncs go to the stale slots for the worker to remove, inline
    // events go to the diagnostic ring.
    void publishArmContextFromCallback(const SfxArmContext& context);
    void pushStaleSyncHandle(quint32 handle);
    void drainStaleSyncHandles();
    void drainDeferredMixerSync();
    void drainSfxCallbackEvents();
    // The chain watchdog: an armed sync the cursor is past (dead), an active scheduler with
    // nothing armed while groups remain, or a decode<->chart mapping that drifted a full
    // second from the GUI's chart second. The first two are repaired in place (late groups
    // played inline, the next one re-armed from the existing anchor); the last re-anchors.
    void runSfxChainWatchdog(double referenceChartSecond, bool hasReference);
    // Worker-side upkeep for the callback-driven chain: callback diagnostics, dead-sync
    // removal, a deferred sync, the watchdog. Runs from the commands PreviewAudioWorker
    // executes on every playback tick (with the GUI's chart second as reference) and from
    // its health tick (without one).
    void serviceSfxScheduler(double referenceChartSecond, bool hasReference);
    void logSfxCallbackEvent(
        const miacode::preview_audio::bass::SfxCallbackEvent& event) const;
    // Must be called with schedulerMutex_ released.
    void logSfxSchedulerArmFailure(const SfxSchedulerArmFailure& failure) const;
    void stopAllSamples();
    void stopPlaybackSession();
    double authoritativeSecond() const;
    void configureBackgroundTrackForSecond(
        double second,
        const QString& reason,
        miacode::preview_audio::bass::BassDebugRoute route);
    bool maybeStartPendingBackgroundTrack(double second);
    bool playKindInternal(
        const QString& kind,
        double gain = 1.0,
        int* nativeErrorCode = nullptr);
    // What reconcileTouchholdVoice() did, so the caller can log it after releasing
    // schedulerMutex_. The audio-thread path reaches this function from triggerGroup(),
    // i.e. under that lock, and a log write there is the stall the buffer-health probe
    // exists to catch. `changed` is false when the voice already belonged to the right
    // span, which is the common case and emits nothing.
    struct TouchholdTransition {
        bool changed = false;
        int owner = -1;
        int previousOwner = -1;
        double second = 0.0;
        double spanStartSecond = -1.0;
    };
    // Pass `out` when holding schedulerMutex_: the transition is recorded rather than
    // logged, and the caller emits it once the lock is gone. With `out` null the function
    // logs directly, which is correct only on the GUI path where no lock is held.
    void reconcileTouchholdVoice(double second, TouchholdTransition* out = nullptr);
    void logTouchholdTransition(const TouchholdTransition& transition) const;
    // Worker paths can collect a compact QString list in playedKindsOut. The mixer
    // callback instead supplies playedSnapshotOut, which records the same successful
    // starts in fixed POD storage for deferred worker-thread formatting.
    void triggerGroup(
        const CollapsedEventGroup& group,
        QString* playedKindsOut = nullptr,
        TouchholdTransition* touchholdOut = nullptr,
        miacode::preview_audio::bass::PlayedSfxSnapshot* playedSnapshotOut = nullptr);
    // Stable bass_status token for the action the mixer sync is armed for. A member
    // rather than a neighbour of retainedPlaybackModeLabel in
    // BassPreviewAudioBackendImpl.h because ScheduledMixerAction is private here.
    static QString scheduledMixerActionLabel(ScheduledMixerAction action);
    void logPlaybackStatus(double authoritativeSecond, double fallbackSecond);
    // Diagnostic-only DSP probe on masterMixer_ (see PreviewAudioOutputGlitchProbe.h).
    // attach/detach bracket the master mixer's own lifetime in initializeAudioEngine /
    // every teardown path (dtor, invalidateOutputDevice); drain runs on the worker
    // thread's existing ~1 Hz sampleHealth() cadence, since the master mixer -- and so
    // the DSP callback -- keeps running for the engine's lifetime independent of
    // whether playback is active.
    void attachOutputGlitchProbe();
    void detachOutputGlitchProbe();
    void drainOutputGlitchEvents();

    // Retained for transport paths that already bracket active playback. The worker reads
    // it when taking the once-per-second BASS sample; it is not an independent producer.
    std::atomic_bool audioHealthPlaybackRunning_{false};
    miacode::preview_audio::PreviewAudioHealthSample latestHealthSample_;
    void logPreparedEventWindow(double startSecond) const;
    QString groupSignature(const CollapsedEventGroup& group) const;
    static void onMixerGroupSync(quint32 handle, quint32 channel, quint32 data, void* user);
    void handleMixerGroupSync(quint32 handle);

    PreviewAudioSettings settings_;
    PreviewTimingSettings timingSettings_;
    PreparedAssetState preparedAssets_;
    TimelineProgramState preparedTimeline_;
    QVector<CollapsedEventGroup> preparedGroups_;
    PreparedPlaybackState preparedPlayback_;
    PlaybackSessionState playbackSession_;
    quint64 playbackTransactionId_ = 0;
    // A1: bumped whenever the BGM cursor is discontinuously repositioned (seek, live rate
    // change) -- see configureBackgroundTrackForSecond and applyPlaybackRateAtChartSecond.
    // Stamped onto every PreviewAudioHealthSample so the underrun advance-rate probe can
    // tell whether two samples came from the same continuous playback segment.
    quint64 backgroundTrackContinuityEpoch_ = 0;
    quint32 deviceSampleRate_ = static_cast<quint32>(miacode::preview_audio::kMixSampleRate);
    double preparedTimelinePlaybackRate_ = 1.0;
    bool engineInitialized_ = false;
    int lastNativeErrorCode_ = 0;
    quint32 masterMixer_ = 0;
    double masterMixerOutputBufferSeconds_ = 0.0;
    // Byte rate of masterMixer_'s format (BASS_ChannelSeconds2Bytes(mixer, 1.0)), so the
    // scheduler can convert cursor distances to seconds with plain arithmetic while it
    // holds schedulerMutex_ or runs on the mixer thread, instead of calling back into BASS.
    double masterMixerBytesPerSecond_ = 0.0;
    double mixerBytesToSeconds(quint64 bytes) const
    {
        return masterMixerBytesPerSecond_ > 0.0
            ? static_cast<double>(bytes) / masterMixerBytesPerSecond_
            : 0.0;
    }
    // Signed lead (ms) of an armed sync's target over the decode cursor; 0 when nothing is armed.
    double scheduledGroupSyncLeadMs(
        quint64 targetPosition,
        quint64 decodePosition,
        ScheduledMixerAction action) const;
    // HDSP handle for the output-glitch probe attached to masterMixer_; 0 when not
    // attached. outputGlitchProbeState_ is the audio-thread-owned tracker state the DSP
    // callback mutates -- see BassPreviewOutputGlitchProbeState.h.
    quint32 outputGlitchDspHandle_ = 0;
    miacode::audio::bass_detail::OutputGlitchProbeState outputGlitchProbeState_;
    quint32 pluginAac_ = 0;
    quint32 pluginOpus_ = 0;
    miacode::preview_audio::PreviewBassDeviceLease bassDeviceLease_;
    int bassOutputDeviceIndex_ = -1;
    QString bassOutputEndpointId_;
    bool outputDeviceRebuildRequired_ = false;
    void* bassFxModule_ = nullptr;
    void* bassFxTempoCreate_ = nullptr;
    RetainedPlaybackMode retainedPlaybackMode_ = RetainedPlaybackMode::None;
    RetainedBgmState retainedBgmState_ = RetainedBgmState::NoneLoaded;
    bool initWindowActive_ = true;
    quint64 transportReadyGeneration_ = 0;
    bool trackMissingAfterLoadLogged_ = false;
    std::atomic_bool shuttingDown_ = false;
    // Shared with the BASS mixer callback. The callback only uses tryLock(): contention is
    // handed back to PreviewAudioWorker through deferredMixerSyncHandle_ and replayed by
    // serviceSfxScheduler(), so the real-time thread never waits on this mutex. Callback
    // diagnostics use sfxCallbackEventRing_ and are formatted/written by the worker.
    // Worker-owned paths may take the lock normally, but must not format or write logs
    // while holding it.
    mutable QMutex schedulerMutex_;
    std::atomic<quint32> deferredMixerSyncHandle_{0};
    miacode::preview_audio::bass::SfxCallbackEventRing sfxCallbackEventRing_;
    // Dead syncs discovered by the mixer callback's arm pass. They can never fire, so they
    // only leak until the worker removes them outside schedulerMutex_; a full array simply
    // leaks the extra handle until the master mixer is freed.
    std::array<std::atomic<quint32>, 16> staleSyncHandles_{};
    // Chain health counters for bass_status / disarm rows: cumulative over the backend's
    // lifetime, so a capture can diff them across sessions.
    std::atomic<quint64> sfxInlineTriggerCount_{0};
    std::atomic<quint64> sfxInlineSkipCount_{0};
    std::atomic<quint64> sfxWatchdogRecoveryCount_{0};
    std::atomic<quint64> sfxDeferredSyncCount_{0};
    // steady_clock ns of the last group actually triggered (any path); 0 = never.
    std::atomic<qint64> sfxLastTriggerMonotonicNs_{0};
    int sfxClockDivergenceStrikes_ = 0;
    quint32 scheduledGroupSync_ = 0;
    int scheduledGroupIndex_ = -1;
    ScheduledMixerAction scheduledMixerAction_ = ScheduledMixerAction::None;
    // Master decode position scheduledGroupSync_ fires at; lets the worker tell a pending
    // sync from one the cursor has already passed.
    quint64 scheduledGroupTargetPosition_ = 0;
    bool sfxSchedulerActive_ = false;
    SfxSchedulerArmFailure sfxSchedulerArmFailure_;
    miacode::preview_audio::bass::SfxSchedulerAnchor sfxSchedulerAnchor_;
    quint64 sfxSchedulerAnchorDecodePosition_ = 0;
    QHash<QString, Sample*> samplesByKind_;
    Sample* backgroundTrackSample_ = nullptr;
    Sample* touchholdSample_ = nullptr;
    int touchholdOwnerSpanIndex_ = -1;
    std::unique_ptr<Sample> answerSample_;
    std::unique_ptr<Sample> judgeSample_;
    std::unique_ptr<Sample> judgeBreakSample_;
    std::unique_ptr<Sample> slideSample_;
    std::unique_ptr<Sample> breakSample_;
    std::unique_ptr<Sample> breakSlideStartSample_;
    std::unique_ptr<Sample> breakSlideFinishSample_;
    std::unique_ptr<Sample> breakSlideTailBreakSample_;
    std::unique_ptr<Sample> judgeBreakSlideSample_;
    std::unique_ptr<Sample> exSample_;
    std::unique_ptr<Sample> touchSample_;
    std::unique_ptr<Sample> touchholdSampleOwner_;
    std::unique_ptr<Sample> fireworkSample_;
    std::unique_ptr<Sample> clockSample_;  // clock_count count-in (audition only)
    std::unique_ptr<Sample> trackStartSample_;  // 片头 opening jingle (audition only)
    std::unique_ptr<Sample> backgroundTrackSampleOwner_;
};

namespace miacode::preview_audio {

// The BASS backend packaged for installPreviewAudioBackendProvider(): creates
// BassPreviewAudioBackend on the worker thread and pauses the armed output on
// a device change (PreviewBassEmergencyPause).
PreviewAudioBackendProvider bassPreviewAudioBackendProvider();

}  // namespace miacode::preview_audio
