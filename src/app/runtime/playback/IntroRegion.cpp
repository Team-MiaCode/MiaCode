#include "app/runtime/playback/PlaybackCoordinator.h"
#include "app/runtime/Shared.h"

#include "app/services/ExportPagePort.h"
#include "audio/QtPreviewSfxRuntime.h"
#include "core/chart/parser/SimaiParser.h"
#include "app/quick_shell/QuickShellPreviewCompositeSurface.h"
#include "app/quick_shell/QuickShellPreviewSurfacePolicy.h"
#include "core/chart/ChartAssetPaths.h"
#include "common/DebugLog.h"
#include "common/DebugOptions.h"
#include "common/OperationLog.h"
#include "core/video/PreviewGameplayConfig.h"
#include "core/video/PreviewInteractionConfig.h"
#include "preview/runtime/PreviewRuntime.h"
#include "preview/stage_media/PreviewStageMediaHost.h"
#include "export/video_export/FontLibrary.h"
#include "core/chart/IntroConfig.h"
#include "export/video_export/VideoExportController.h"
#include "core/scene/PreviewOpacityCurves.h"
#include "core/scene/PreviewProgressStatsCache.h"
#include "core/chart/transform/ChartBatchTransform.h"
#include "core/chart/transform/ChartNormalization.h"
#include "timeline/quick/TimelineQuickStateBridge.h"
#include "core/analysis/MuriAnalyzer.h"
#include "core/analysis/MuriPanelEntries.h"
#include "core/analysis/MuriStaticChecker.h"

#include <QtCore>
#include <QtGui>

#include <cmath>

#include <cstdio>  // G2 Diag: std::snprintf for sync rate-change beacon lines
#include "app/runtime/playback/Playback.Internal.h"

using namespace miacode::runtime::shared;
using namespace miacode::runtime::playback_detail;

namespace {
// Spec -> IntroOverlay.qml banner template (the JSON layout the export overlay
// mount also reads). Kept file-local; introBannerTrackMap is the shared inline
// in VideoExportController.h.
QVariantMap introLeadInBannerTemplateMap()
{
    QVariantMap templateMap;
    QFile templateFile(QString::fromLatin1(miacode::intro::kBannerTemplateUrl).mid(3));  // "qrc:" -> ":"
    if (templateFile.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(templateFile.readAll());
        if (doc.isObject()) {
            templateMap = doc.object().toVariantMap();
        }
    }
    return templateMap;
}

// PV-preview audition: the transport start is nudged past chart 0 so it never
// takes the visual lead-in, and the song's fade gain moves in 1/32 steps.
constexpr double kPvSegmentMinimumStartSecond = 0.001;
constexpr double kPvSegmentGainSteps = 32.0;

double smoothstep01(double t)
{
    t = qBound(0.0, t, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

// The export's music envelope (VideoExportAudioRenderPlan / BassExportAudioBackend)
// at `elapsedSeconds` into the intro: fade in from 0, fade out ending when the
// wipe fully covers the screen.
double pvSegmentTrackGain(double elapsedSeconds)
{
    const double endSeconds = miacode::intro::kPvPreviewHoldSeconds + miacode::intro::kPvPreviewAudioTailSeconds;
    const double fadeIn = smoothstep01(elapsedSeconds / miacode::intro::kPvPreviewAudioFadeInSeconds);
    const double fadeOut = 1.0 - smoothstep01(
        (elapsedSeconds - (endSeconds - miacode::intro::kPvPreviewAudioFadeOutSeconds))
        / miacode::intro::kPvPreviewAudioFadeOutSeconds);
    return qBound(0.0, qMin(fadeIn, fadeOut), 1.0);
}
}  // namespace

// qmlExportSession_ is a RuntimeContext::Ui field (see SessionMembers.inc), so
// this only touches ui_.
bool miacode::runtime::PlaybackCoordinator::currentExportIntroLeadInSpec(IntroBannerSpec* outSpec) const
{
    // The export session owns the shared 片头 settings. The audition reads them
    // at play time so both single and batch preview stay WYSIWYG.
    if (ui_.qmlExportSession_ != nullptr
        && ui_.qmlExportSession_->pageSessionActive()
        && ui_.qmlExportSession_->previewIntroSpec().enabled) {
        if (outSpec != nullptr) {
            *outSpec = ui_.qmlExportSession_->previewIntroSpec();
        }
        return true;
    }
    return false;
}

bool miacode::runtime::PlaybackCoordinator::exportIntroEnabled() const
{
    // Authoritative, read live from the panel each time (NO cached flag — a
    // cached duration could be reset to 0 by a transient refresh while 添加片头
    // is steadily on, collapsing the slider range; the bug we saw).
    return state_.exportPreviewAuditionActive_ && currentExportIntroLeadInSpec(nullptr);
}

double miacode::runtime::PlaybackCoordinator::exportIntroDurationSeconds() const
{
    IntroBannerSpec spec;
    return currentExportIntroLeadInSpec(&spec) ? introDurationSeconds(spec) : 0.0;
}

double miacode::runtime::PlaybackCoordinator::exportIntroLowerBoundSeconds() const
{
    // The intro occupies negative time [-duration, 0) only while the export
    // audition is up and 添加片头 is on; otherwise the slider starts at 0.
    return exportIntroEnabled() ? -exportIntroDurationSeconds() : 0.0;
}

void miacode::runtime::PlaybackCoordinator::setupExportIntroOverlayData()
{
    if (state_.scene_ == nullptr) {
        return;
    }
    IntroBannerSpec spec;
    if (!currentExportIntroLeadInSpec(&spec)) {
        return;
    }
    // backgroundImage is ALWAYS the 曲绘 jacket (it feeds the card's jacket slot
    // AND the backdrop fallback) — mirror the export mount, which passes jacketUrl
    // here and routes the 片头 tab's 背景虚化/自定义背景/卡片阴影 through the style
    // map (introBannerStyleMap → backdropImage/backdropBlurEnabled/cardShadowEnabled).
    // Passing the custom backdrop as backgroundImage (the old behavior) wrongly
    // replaced the card jacket and ignored the blur toggle.
    const QUrl jacketUrl = miacode::chart_assets::displayBackgroundImageUrl(spec.jacketPath);
    // Overlay the dialog's difficulty-card custom fonts onto the lead-in template
    // copy so the main-timeline audition matches the export (same FontLibrary
    // override as the export mount + the dialog preview).
    QVariantMap templateMap = introLeadInBannerTemplateMap();
    miacode::video_export::applyBannerFontOverride(templateMap, spec.fontDisplayPath, spec.fontBodyPath);
    // The preview's own stage PV plays under the PV-preview hold, as the export
    // composites its PV segment under the overlay.
    QVariantMap style = introBannerStyleMap(spec, previewStageMediaRouteHasVideo());
    // Preview only: dragging the PV segment window fades the card so the PV
    // under it stays readable.
    style.insert(QStringLiteral("cardDimmed"), ui_.qmlExportSession_->introPvSegmentDragging());
    state_.scene_->setIntroOverlayData(
        introBannerTrackMap(spec),
        templateMap,
        jacketUrl,
        QUrl(QString::fromLatin1(miacode::intro::kLogoFallbackUrl)),
        style);
}

void miacode::runtime::PlaybackCoordinator::renderExportIntroFrame(double positionSeconds)
{
    if (state_.scene_ == nullptr) {
        return;
    }
    IntroBannerSpec spec;
    const bool pvPreview = currentExportIntroLeadInSpec(&spec) && spec.pvPreview;
    // position in [-duration, 0] maps to authoring frame 0..durationFrames.
    const double into = positionSeconds + miacode::intro::introDurationSeconds(pvPreview);
    const int frame = qBound(
        0, qRound(into * static_cast<double>(miacode::intro::kAuthoringFps)),
        miacode::intro::introDurationFrames(pvPreview));
    state_.scene_->setIntroStillFrame(
        !state_.exportIntroPvSegmentActive_ && !state_.exportIntroLeadInActive_);
    state_.scene_->setIntroOverlayFrame(frame, true);
    state_.scene_->setIntroHidesChart(pvPreview && frame < miacode::intro::kPvPreviewHoldFrames);
}

void miacode::runtime::PlaybackCoordinator::enterExportIntroRegion(double positionSeconds)
{
    IntroBannerSpec spec;
    if (state_.scene_ == nullptr || !currentExportIntroLeadInSpec(&spec) || !exportIntroEnabled()) {
        return;
    }
    if (state_.exportIntroPvSegmentActive_) {
        stopExportIntroPvSegment();
    }
    if (state_.playing_) {
        stopQtPreviewPlayback(true);  // freeze the chart behind the overlay
    }
    const bool firstEntry = !state_.exportIntroRegionActive_;
    if (firstEntry) {
        setupExportIntroOverlayData();
    }
    state_.exportIntroRegionActive_ = true;
    playbackState_.previewTransportState_ = miacode::PlaybackTransportState::Paused;
    const double durationSeconds = introDurationSeconds(spec);
    state_.exportIntroPlayheadSeconds_ = qBound(-durationSeconds, positionSeconds, 0.0);
    // The paused chart parks where the intro shows it: inside the PV segment
    // during the PV-preview hold (its stage PV is on screen), else at chart 0.
    const double elapsedSeconds = state_.exportIntroPlayheadSeconds_ + durationSeconds;
    const double parkSecond = spec.pvPreview && elapsedSeconds < miacode::intro::kPvPreviewHoldSeconds
        ? spec.pvPreviewStartSeconds + elapsedSeconds
        : 0.0;
    if (firstEntry || qAbs(parkSecond - state_.exportIntroParkedChartSecond_) > 1e-4) {
        requestPausedPreviewSeek(parkSecond, false, true);
        state_.exportIntroParkedChartSecond_ = parkSecond;
    }
    renderExportIntroFrame(state_.exportIntroPlayheadSeconds_);
    publishPreviewPlayhead();
}

void miacode::runtime::PlaybackCoordinator::exitExportIntroRegion()
{
    const bool wasActive = state_.exportIntroRegionActive_ || state_.exportIntroLeadInActive_
        || state_.exportIntroPvSegmentActive_;
    stopExportIntroPvSegment();
    state_.exportIntroAuditionActive_ = false;
    state_.exportIntroLeadInActive_ = false;
    state_.exportIntroRegionActive_ = false;
    state_.exportIntroParkedChartSecond_ = -1.0;
    // Drop the negative playhead so a later stray read can't resurrect a frozen
    // intro position (the region flags above are authoritative; this is hygiene).
    state_.exportIntroPlayheadSeconds_ = 0.0;
    if (state_.exportIntroLeadInTimer_ != nullptr) {
        state_.exportIntroLeadInTimer_->stop();
    }
    if (wasActive && state_.scene_ != nullptr) {
        state_.scene_->clearIntroOverlay(true);
    }
    if (wasActive) {
        updatePauseButtonAppearance();
    }
    if (state_.previewTransportState_ == miacode::PlaybackTransportState::Playing) {
        playbackState_.previewTransportState_ = miacode::PlaybackTransportState::Paused;
    }
}

bool miacode::runtime::PlaybackCoordinator::exportIntroLeadInPlaying() const
{
    return state_.exportIntroLeadInActive_;
}

void miacode::runtime::PlaybackCoordinator::cancelExportIntroLeadIn()
{
    // Full exit (clears the overlay) — used by stop / teardown.
    exitExportIntroRegion();
}

void miacode::runtime::PlaybackCoordinator::pauseExportIntroAdvance()
{
    // Only user transport actions pause the intro; they all end an audition.
    state_.exportIntroAuditionActive_ = false;
    if (state_.exportIntroPvSegmentActive_) {
        // Re-entering the region stops the segment and parks the PV on the
        // paused frame.
        enterExportIntroRegion(state_.exportIntroPlayheadSeconds_);
        updatePauseButtonAppearance();
        return;
    }
    if (!state_.exportIntroLeadInActive_) {
        return;
    }
    haltExportIntroLeadInTimer();
    // Keep the region + static frame so the paused intro stays on screen.
    playbackState_.previewTransportState_ = miacode::PlaybackTransportState::Paused;
    updatePauseButtonAppearance();
}

void miacode::runtime::PlaybackCoordinator::startExportIntroAdvance(double fromPositionSeconds)
{
    IntroBannerSpec spec;
    if (state_.scene_ == nullptr || !currentExportIntroLeadInSpec(&spec) || !exportIntroEnabled()) {
        return;
    }
    enterExportIntroRegion(fromPositionSeconds);
    state_.exportIntroAdvanceFromSeconds_ = state_.exportIntroPlayheadSeconds_;
    const double elapsedSeconds = state_.exportIntroPlayheadSeconds_ + introDurationSeconds(spec);

    // PV preview: the song + PV segment runs on the real transport until its
    // music has faded out under the wipe; the timer below covers the rest.
    if (spec.pvPreview
        && elapsedSeconds < miacode::intro::kPvPreviewHoldSeconds + miacode::intro::kPvPreviewAudioTailSeconds) {
        startExportIntroPvSegment(spec.pvPreviewStartSeconds, elapsedSeconds);
        if (state_.exportIntroPvSegmentActive_) {
            return;
        }
    }

    // Opening jingle — only when advancing from at/near the intro head. Played
    // through the SAME BASS audition path as the note SFX / clock count-in: the
    // QSoundEffect path was inaudible on this Windows/Qt build (GUI 2026-06-16),
    // while audition() is proven (clock_count works). The SFX runtime is already
    // prepared by installExportPreviewAuditionScene; ensure it anyway (idempotent).
    // The PV preview has no opening jingle.
    if (!spec.pvPreview && elapsedSeconds <= 0.1) {
        ensurePreviewSfxRuntimePrepared(state_);
        if (state_.previewSfxRuntime_ != nullptr) {
            state_.previewSfxRuntime_->audition(QStringLiteral("track_start"), 1.0);
        }
    }

    if (state_.exportIntroLeadInTimer_ == nullptr) {
        state_.exportIntroLeadInTimer_ = new QTimer(&owner_);
        state_.exportIntroLeadInTimer_->setInterval(16);  // ~60 fps overlay frame stepping
        QObject::connect(state_.exportIntroLeadInTimer_, &QTimer::timeout, &owner_, [this]() {
            tickExportIntroLeadIn();
        });
    }
    state_.exportIntroLeadInActive_ = true;
    renderExportIntroFrame(state_.exportIntroPlayheadSeconds_);
    playbackState_.previewTransportState_ = miacode::PlaybackTransportState::Playing;
    state_.exportIntroLeadInElapsed_.restart();
    state_.exportIntroLeadInTimer_->start();
    updatePauseButtonAppearance();
}

void miacode::runtime::PlaybackCoordinator::tickExportIntroLeadIn()
{
    if (!state_.exportIntroLeadInActive_) {
        return;
    }
    const double elapsedSeconds = static_cast<double>(state_.exportIntroLeadInElapsed_.elapsed()) / 1000.0;
    const double position = state_.exportIntroAdvanceFromSeconds_ + elapsedSeconds;
    if (finishExportIntroAuditionIfDue(position + exportIntroDurationSeconds())) {
        return;
    }
    if (position >= 0.0) {
        // Crossed 0 -> hand off to the normal chart audition from the chart head.
        exitExportIntroRegion();
        startQtPreviewPlayback(0.0, true);
        return;
    }
    state_.exportIntroPlayheadSeconds_ = position;
    renderExportIntroFrame(position);
    publishPreviewPlayhead();
}

void miacode::runtime::PlaybackCoordinator::startExportIntroPvSegment(double startSeconds, double elapsedSeconds)
{
    state_.exportIntroPvSegmentActive_ = true;
    state_.exportIntroPvSegmentStartSeconds_ = qMax(0.0, startSeconds);
    state_.exportIntroPvSegmentTrackGain_ = pvSegmentTrackGain(elapsedSeconds);
    state_.exportIntroParkedChartSecond_ = -1.0;
    // startQtPreviewPlayback re-applies the levels, now in the intro-segment
    // mode (note SFX silent, song on the fade envelope). A start exactly at
    // chart 0 would take the visual lead-in, so nudge it past the tolerance.
    const double chartSecond = qMax(kPvSegmentMinimumStartSecond,
                                    state_.exportIntroPvSegmentStartSeconds_ + elapsedSeconds);
    if (!startQtPreviewPlayback(chartSecond, false)) {
        // The chart is not ready to play yet. Drop the deferred start it queued
        // (it would later play the song outside the intro) and let the caller
        // run the intro silently on its timer.
        state_.pendingPreviewPlaybackStart_ = false;
        state_.exportIntroPvSegmentActive_ = false;
        state_.exportIntroPvSegmentTrackGain_ = 1.0;
        preview_.applyPreviewAudioSettingsToRuntime();
        return;
    }
    // Stop returns to the intro, not into the song.
    state_.qtPreviewPlaybackReturnSecond_ = 0.0;
    renderExportIntroFrame(state_.exportIntroPlayheadSeconds_);
    updatePauseButtonAppearance();
}

void miacode::runtime::PlaybackCoordinator::tickExportIntroPvSegment(double chartSecond)
{
    syncPreviewStageMediaRoutePlayback(chartSecond);
    setPreviewStageMediaRouteObservedPlayheadSecond(chartSecond);
    const double durationSeconds = exportIntroDurationSeconds();
    const double elapsedSeconds = chartSecond - state_.exportIntroPvSegmentStartSeconds_;
    state_.exportIntroPlayheadSeconds_ = qBound(-durationSeconds, elapsedSeconds - durationSeconds, 0.0);
    renderExportIntroFrame(state_.exportIntroPlayheadSeconds_);
    // Quantised so the level dispatch only runs when the envelope moves audibly.
    const double gain = std::round(pvSegmentTrackGain(elapsedSeconds) * kPvSegmentGainSteps) / kPvSegmentGainSteps;
    if (gain != state_.exportIntroPvSegmentTrackGain_) {
        state_.exportIntroPvSegmentTrackGain_ = gain;
        preview_.applyPreviewAudioSettingsToRuntime();
    }
    publishPreviewPlayhead();
    if (elapsedSeconds >= miacode::intro::kPvPreviewHoldSeconds + miacode::intro::kPvPreviewAudioTailSeconds) {
        if (finishExportIntroAuditionIfDue(elapsedSeconds)) {
            return;
        }
        // The music has faded out and the wipe covers the screen: park the
        // chart at 0 unseen and let the timer run the rest of the wipe.
        startExportIntroAdvance(state_.exportIntroPlayheadSeconds_);
    }
}

void miacode::runtime::PlaybackCoordinator::stopExportIntroPvSegment()
{
    if (!state_.exportIntroPvSegmentActive_) {
        return;
    }
    state_.exportIntroPvSegmentActive_ = false;
    state_.exportIntroPvSegmentTrackGain_ = 1.0;
    state_.exportIntroParkedChartSecond_ = -1.0;
    if (state_.playing_ || state_.previewStartupSyncPending_ || state_.previewLateVideoStartPending_) {
        stopQtPreviewPlayback(true);
    }
    preview_.applyPreviewAudioSettingsToRuntime();
}

void miacode::runtime::PlaybackCoordinator::haltExportIntroLeadInTimer()
{
    state_.exportIntroLeadInActive_ = false;
    if (state_.exportIntroLeadInTimer_ != nullptr) {
        state_.exportIntroLeadInTimer_->stop();
    }
}

bool miacode::runtime::PlaybackCoordinator::exportIntroAuditionPlaying() const
{
    return state_.exportIntroAuditionActive_;
}

void miacode::runtime::PlaybackCoordinator::setExportIntroAuditionPlaying(bool playing)
{
    if (!playing) {
        if (!state_.exportIntroAuditionActive_) {
            return;
        }
        // Stop returns to where the audition started.
        state_.exportIntroAuditionActive_ = false;
        haltExportIntroLeadInTimer();
        enterExportIntroRegion(state_.exportIntroAuditionReturnSeconds_);
        updatePauseButtonAppearance();
        return;
    }
    IntroBannerSpec spec;
    if (state_.scene_ == nullptr || !currentExportIntroLeadInSpec(&spec) || !spec.pvPreview
        || !exportIntroEnabled()) {
        return;
    }
    // Play from the picked moment when the user seeked inside the segment,
    // from the segment head after it moved. Ending parks back on the frame
    // that was showing (paused frames are stills, so never a black frame).
    const double durationSeconds = introDurationSeconds(spec);
    const double elapsedSeconds = state_.exportIntroRegionActive_
        ? state_.exportIntroPlayheadSeconds_ + durationSeconds
        : -1.0;
    const bool pausedInSegment =
        elapsedSeconds >= 0.0 && elapsedSeconds < miacode::intro::kPvPreviewHoldSeconds;
    const bool fromHead = ui_.qmlExportSession_->introAuditionFromHead();
    const double startElapsedSeconds = pausedInSegment && !fromHead ? elapsedSeconds : 0.0;
    const double returnElapsedSeconds = pausedInSegment ? elapsedSeconds : 0.0;
    haltExportIntroLeadInTimer();
    state_.exportIntroAuditionStartSeconds_ = startElapsedSeconds - durationSeconds;
    state_.exportIntroAuditionReturnSeconds_ = returnElapsedSeconds - durationSeconds;
    state_.exportIntroAuditionActive_ = true;
    startExportIntroAdvance(state_.exportIntroAuditionStartSeconds_);
}

bool miacode::runtime::PlaybackCoordinator::finishExportIntroAuditionIfDue(double elapsedSeconds)
{
    if (!state_.exportIntroAuditionActive_
        || elapsedSeconds < miacode::intro::kPvPreviewHoldSeconds + miacode::intro::kPvPreviewAudioTailSeconds) {
        return false;
    }
    const bool loop = ui_.qmlExportSession_ != nullptr && ui_.qmlExportSession_->introAuditionLoop();
    if (loop) {
        haltExportIntroLeadInTimer();
        startExportIntroAdvance(state_.exportIntroAuditionStartSeconds_);
    } else {
        state_.exportIntroAuditionActive_ = false;
        haltExportIntroLeadInTimer();
        enterExportIntroRegion(state_.exportIntroAuditionReturnSeconds_);
    }
    updatePauseButtonAppearance();
    return true;
}

bool miacode::runtime::PlaybackCoordinator::handleExportIntroSliderSeek(double second)
{
    if (!exportIntroEnabled()) {
        return false;
    }
    if (second >= 0.0) {
        // Back in the chart region: drop the overlay and let the normal seek run.
        if (state_.exportIntroRegionActive_) {
            exitExportIntroRegion();
        }
        return false;
    }
    // In the intro region: render the frame statically (no chart audio/advance).
    state_.exportIntroAuditionActive_ = false;
    if (state_.exportIntroLeadInActive_) {
        pauseExportIntroAdvance();
    }
    enterExportIntroRegion(second);
    return true;
}

void miacode::runtime::PlaybackCoordinator::refreshExportIntroState()
{
    const bool introOn = exportIntroEnabled();
    if (!introOn) {
        // 添加片头 off (or left the page): leave the intro region, back to chart 0.
        if (state_.exportIntroRegionActive_ || state_.exportIntroLeadInActive_) {
            exitExportIntroRegion();
            miacode::runtime::shared::writePreviewPauseSecond(
                playbackState_.pauseSecond_, 0.0, playbackState_.playing_, "refresh_export_intro_state");
            seekPreviewDiscreteToSecond(0.0, true);
        }
        publishPreviewPlayhead();
        return;
    }
    if (state_.exportIntroRegionActive_) {
        // Refresh the overlay with the new 片头 settings, keep the position. A
        // running PV segment pauses so its PV re-parks on the new segment.
        setupExportIntroOverlayData();
        if (state_.exportIntroLeadInActive_) {
            renderExportIntroFrame(state_.exportIntroPlayheadSeconds_);
        } else {
            state_.exportIntroAuditionActive_ = false;
            enterExportIntroRegion(state_.exportIntroPlayheadSeconds_);
            updatePauseButtonAppearance();
        }
    } else if (!state_.playing_ && qAbs(state_.pauseSecond_) <= 0.05) {
        // Default the playhead to the intro head so the user sees it first.
        enterExportIntroRegion(-exportIntroDurationSeconds());
    }
}

void miacode::runtime::PlaybackCoordinator::setExportAuditionClockSchedule(int clockCount, double clockBpm)
{
    // clock_count count-in for the export audition. clock ticks live at chart-time
    // [0, count*beat) (beat = 60/clockBpm), mirroring the export's
    // appendClockCountPlaybacks — they sound on the chart audition AFTER the 片头
    // hands off at chart 0.
    const bool valid = clockCount > 0 && qIsFinite(clockBpm) && clockBpm > 0.0;
    state_.exportAuditionClockCount_ = valid ? clockCount : 0;
    state_.exportAuditionClockBeatSeconds_ = valid ? (60.0 / clockBpm) : 0.0;
    state_.exportAuditionClockNextIndex_ = 0;
}

void miacode::runtime::PlaybackCoordinator::clearExportAuditionClockSchedule()
{
    state_.exportAuditionClockCount_ = 0;
    state_.exportAuditionClockBeatSeconds_ = 0.0;
    state_.exportAuditionClockNextIndex_ = 0;
}

void miacode::runtime::PlaybackCoordinator::resetExportAuditionClockCursor(double startSecond)
{
    // Skip ticks that already elapsed before startSecond WITHOUT firing them, so
    // resuming mid-chart or seeking past the count-in doesn't replay it.
    int index = 0;
    if (state_.exportAuditionClockBeatSeconds_ > 0.0) {
        while (index < state_.exportAuditionClockCount_
               && index * state_.exportAuditionClockBeatSeconds_
                      + kTimelineZeroSecondTolerance < startSecond) {
            ++index;
        }
    }
    state_.exportAuditionClockNextIndex_ = index;
}

void miacode::runtime::PlaybackCoordinator::maybeFireExportAuditionClockTicks(double second)
{
    if (!state_.exportPreviewAuditionActive_
        || state_.exportAuditionClockCount_ <= 0
        || state_.exportAuditionClockBeatSeconds_ <= 0.0
        || state_.previewSfxRuntime_ == nullptr) {
        return;
    }
    while (state_.exportAuditionClockNextIndex_ < state_.exportAuditionClockCount_) {
        const double tickSecond =
            state_.exportAuditionClockNextIndex_ * state_.exportAuditionClockBeatSeconds_;
        if (tickSecond > second + kTimelineZeroSecondTolerance) {
            break;  // not yet due
        }
        // Don't machine-gun a backlog: skip (without playing) any tick we blew past
        // by more than one beat (a forward seek during playback). The downbeat at 0
        // and on-time ticks (≤ one frame late) still fire — gain 1.0 so the loaded
        // clock sample's own clock-volume level applies.
        if (second - tickSecond <= state_.exportAuditionClockBeatSeconds_) {
            state_.previewSfxRuntime_->audition(QStringLiteral("clock"), 1.0);
        }
        ++state_.exportAuditionClockNextIndex_;
    }
}
