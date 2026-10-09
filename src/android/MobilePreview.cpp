#include "MobilePreview.h"
#include "MobileSfxOutput.h"
#include "timeline/TimelineMarkerOffset.h"
#include "common/AssetPaths.h"
#include "common/IntroConfig.h"
#include "common/ContentDurationConfig.h"
#include "timeline/TimelineQuickModel.h"
#include "common/WaveformCache.h"
#include "tools/muri/MuriAnalyzer.h"
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QDebug>
#include <QLoggingCategory>
#include <QUrl>
#include <QQuickItem>

namespace miacode::android {
Q_LOGGING_CATEGORY(mobilePreviewLog, "miacode.android.preview", QtWarningMsg)
MobilePreview::MobilePreview(AndroidDocumentSession* document, QObject* parent)
    : QObject(parent), document_(document), runtime_(this), audio_(this), audioOutput_(this), video_(this), sfx_(std::make_unique<MobileSfxOutput>(this)),
      statisticsCache_(std::make_shared<miacode::preview::scene::PreviewProgressStatsCache>())
{
    audio_.setAudioOutput(&audioOutput_);
    audioOutput_.setVolume(1);
    runtime_.setSkinDirectory(miacode::assets::assetPath("skin/skinDX"));
    runtime_.setProgressStatsCache(statisticsCache_);
    runtime_.setStageMediaPresentationMode(miacode::preview::scene::PreviewStageMediaPresentationMode::ExternalQuickMediaItem);
    timer_.setInterval(16);
    timer_.setTimerType(Qt::PreciseTimer);
    presentationClock_.start();
    connect(&videoFrames_, &MobileVideoFrameRouter::framePresented, this, [this](bool valid) {
        if (hasVideoFrame_ == valid) return;
        hasVideoFrame_ = valid;
        emit mediaChanged();
    });
    connect(&timer_, &QTimer::timeout, this, &MobilePreview::tick);
    connect(sfx_.get(), &MobileSfxOutput::readyChanged, this, [this] { if (playing_) startMediaPlayback(); });
    connect(sfx_.get(), &MobileSfxOutput::failed, this, [this](const QString& error) { setPlaying(false); emit mediaError(error); });
    connect(sfx_.get(), &MobileSfxOutput::outputDeviceChanged, this, [this] { setPlaying(false); });
    connect(document_, &AndroidDocumentSession::documentStateChanged, this, &MobilePreview::refreshDocument);
    connect(document_, &AndroidDocumentSession::documentReplaced, this, [this] {
        playbackEntry_ = 0;
        emit transportChanged();
    });
    connect(document_, &AndroidDocumentSession::mediaAssetsChanged, this, &MobilePreview::refreshMedia);
    connect(&audio_, &QMediaPlayer::positionChanged, this, [this](qint64 ms) {
        audioAnchor_ = ms / 1000.0;
        audioClockAge_.restart();
    });
    connect(&audio_, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error, const QString& message) {
        setPlaying(false);
        emit mediaError(message);
    });
    connect(&audio_, &QMediaPlayer::durationChanged, this, [this](qint64) { refreshDuration(); });
    refreshDocument();
}
MobilePreview::~MobilePreview() {
    video_.setVideoSink(nullptr);
    videoFrames_.detach();
}
void MobilePreview::setCanvasFrameRate(PreviewCanvasFrameRateMode mode, double refreshHz) {
    canvasCadence_.configure(mode, refreshHz);
    // Audio/SFX transport still samples at least 60 Hz. Rendering can run at
    // 30 Hz independently, or at the available 120 Hz without a fixed 16ms cap.
    timer_.setInterval(qMin(16, canvasCadence_.pollingIntervalMs()));
    runtime_.setPlayheadSeconds(exportIntroEnabled_ && position_ < 0 ? 0 : position_);
}
void MobilePreview::setStageMediaFrameRate(PreviewCanvasFrameRateMode mode, double refreshHz) {
    videoFrames_.setFrameRate(mode, refreshHz);
}
void MobilePreview::setCanvasAspectRatio(double ratio) {
    const double normalized = qIsFinite(ratio) && ratio > 0 ? qBound(1.0, ratio, 3.0) : 1.0;
    if (qAbs(canvasAspectRatio_ - normalized) <= 1e-6) return;
    canvasAspectRatio_ = normalized;
    emit canvasAspectRatioChanged();
}
void MobilePreview::applyAudioSettings(const PreviewAudioSettings& settings) {
    normalAudioSettings_ = settings;
    updateAudioLevels();
}
double MobilePreview::lowerBoundSeconds() const {
    return exportAuditionActive_ ? (exportIntroEnabled_ ? -intro::kDurationSeconds : 0) : qMin(0.0, first_);
}
void MobilePreview::setIntroSoundFile(const QString& fileName) { sfx_->setIntroSoundFile(fileName); }
void MobilePreview::configureExportAudition(bool active, bool introEnabled, int clockCount, double clockBpm) {
    introEnabled = active && introEnabled;
    const bool introChanged = exportIntroEnabled_ != introEnabled || exportAuditionActive_ != active;
    const bool wasInIntro = exportIntroEnabled_ && position_ < 0;
    exportIntroEnabled_ = introEnabled;
    exportAuditionActive_ = active;
    sfx_->configureExportAudition(introEnabled, active ? clockCount : 0, clockBpm);
    if (introChanged) {
        if (!introEnabled && wasInIntro) { setPlaying(false); setPositionSeconds(0); }
        else if (position_ < lowerBoundSeconds()) { setPlaying(false); setPositionSeconds(lowerBoundSeconds()); }
        emit transportChanged();
    }
}
void MobilePreview::updateAudioLevels() {
    const auto settings = latencySfxPercent_ < 0 ? normalAudioSettings_
        : makePreviewLatencyAuditionLevels(normalAudioSettings_, latencySfxPercent_);
    audioOutput_.setVolume(previewTrackVolume(settings)); sfx_->setLevels(settings);
}
void MobilePreview::setLatencyAuditionVolume(int percent) {
    latencySfxPercent_ = percent < 0 ? -1 : qBound(0, percent, 100);
    updateAudioLevels();
}
double MobilePreview::previewChartOffset() const {
    return latencyChartActive() ? latencyOffset_
        : miacode::timeline::offset::parsedFirstSeconds(document_->workspace().document().first);
}
void MobilePreview::setLatencyChart(const QString& text, double offset) {
    if (text.isNull() || !qIsFinite(offset)) return;
    latencyChart_ = text; latencyOffset_ = offset;
    refreshDocument(); emit previewSourceChanged();
}
void MobilePreview::clearLatencyChart() {
    if (!latencyChartActive()) return;
    latencyChart_ = QString();
    refreshDocument(); emit previewSourceChanged();
}
QVariantMap MobilePreview::sfxDiagnostics() const { return sfx_->diagnostics(); }

double MobilePreview::trackDurationSeconds() const {
    const QString path = document_->previewAssetPath("audio");
    if (path.isEmpty()) return 0;
    if (decodedTrackDuration_ > 0 && decodedTrackPath_ == waveform::normalizeTrackPath(path))
        return decodedTrackDuration_;
    return audio_.source() == QUrl::fromLocalFile(path) ? qMax(0.0, audio_.duration() / 1000.0) : 0;
}

void MobilePreview::setDecodedTrackDuration(const QString& path, double duration) {
    const QString current = waveform::normalizeTrackPath(document_->previewAssetPath("audio"));
    if (current.isEmpty() || waveform::normalizeTrackPath(path) != current || !qIsFinite(duration) || duration < 0)
        return;
    decodedTrackPath_ = current;
    decodedTrackDuration_ = duration;
    refreshDuration();
}

void MobilePreview::refreshDuration() {
    const double trackDuration = trackDurationSeconds();
    const double next = latencyChartActive() ? (trackDuration > 1 ? trackDuration : 180)
        : content_duration::totalContentDurationSeconds(chartEndSeconds_, trackDuration);
    if (duration_ == next) return;
    duration_ = next;
    sfx_->setEndSecond(rangeEnabled_ ? rangeEnd_ : duration_);
    emit transportChanged();
}

void MobilePreview::refreshDocument()
{
    const auto& document = document_->workspace().document();
    const QString path = QFileInfo(document_->currentFilePath()).absolutePath();
    if (path != projectPath_) setPlaying(false);
    first_ = previewChartOffset();
    const auto parsed = SimaiNativeParser::parseForTimeline(previewChartText(), latencyChartActive()
        ? miacode::simai::SimaiTimingMetadata() : miacode::simai::buildTimingMetadata(document));
    const auto markers = miacode::timeline::offset::shiftedNoteMarkers(parsed.noteMarkers, first_,
        miacode::timeline::offset::NonFiniteHandling::PassThrough);
    runtime_.setNoteMarkers(markers);
    sfx_->configure(markers, rate_);
    runtime_.setMuriRenderOptions(renderOptions_);
    // AnalysisService publishes revision-gated results asynchronously.
    statisticsCache_->rebuild(markers);
    // v2 preview uses the timeline's complete span, including its trailing
    // measure boundary, then applies the shared chart-tail/music policy.
    TimelineQuickModel timeline;
    timeline.rebuildFromText(previewChartText(), first_, latencyChartActive()
        ? miacode::simai::SimaiTimingMetadata() : miacode::simai::buildTimingMetadata(document));
    chartEndSeconds_ = timeline.snapshot().durationSeconds;
    refreshDuration();
    sfx_->setEndSecond(rangeEnabled_ ? rangeEnd_ : duration_);
    const auto* difficulty = document.difficulty(document_->activeDifficulty());
    runtime_.setChartInfo(document.title, document.artist,
        SimaiDocument::difficultyName(document_->activeDifficulty()) + " " + (difficulty ? difficulty->level : QString()),
        difficulty && !difficulty->designer.trimmed().isEmpty() ? difficulty->designer : document.designer);
    if (path != projectPath_) {
        setPlaying(false);
        projectPath_ = path;
        publishPosition(lowerBoundSeconds());
    }
    refreshMedia();
    publishPosition(qBound(lowerBoundSeconds(), position_, duration_));
    emit transportChanged();
}

void MobilePreview::refreshMedia()
{
    const auto localUrl = [](const QString& path) { return path.isEmpty() ? QUrl() : QUrl::fromLocalFile(path); };
    const QUrl audioSource = localUrl(document_->previewAssetPath("audio"));
    if (audio_.source() != audioSource) {
        decodedTrackPath_.clear();
        decodedTrackDuration_ = 0;
        qCDebug(mobilePreviewLog) << "Replacing preview audio source";
        setPlaying(false);
        audioClockAge_.invalidate();
        audio_.setSource(audioSource);
        qCDebug(mobilePreviewLog) << "Preview audio source replaced";
        audio_.setPosition(qMax<qint64>(0, qRound64(position_ * 1000)));
    }
    const QUrl videoSource = localUrl(document_->previewAssetPath("video"));
    if (video_.source() != videoSource) {
        qCDebug(mobilePreviewLog) << "Replacing preview video source";
        video_.pause();
        qCDebug(mobilePreviewLog) << "Preview video paused before replacement";
        hasVideoFrame_ = false;
        // The inner PV output holds its own copy of the decoder's frame.
        // Release both outputs before retiring the old media pipeline.
        clearVideoOutputs();
        qCDebug(mobilePreviewLog) << "Preview video outputs cleared";
        video_.setSource(videoSource);
        qCDebug(mobilePreviewLog) << "Preview video source replaced";
        video_.setPosition(qMax<qint64>(0, qRound64(position_ * 1000)));
        if (playing_ && position_ >= 0 && mediaVisible()) video_.play();
    }
    const QUrl imageSource = localUrl(document_->previewAssetPath("image"));
    if (imageSource_ != imageSource) {
        QImageReader reader(imageSource.toLocalFile());
        const bool readable = !imageSource.isEmpty() && reader.canRead();
        imageSource_ = readable ? imageSource : QUrl();
        if (readable) qInfo() << "Mobile preview image:" << reader.format() << reader.size();
        else if (!imageSource.isEmpty()) emit mediaError(reader.errorString());
    }
    runtime_.setStageMediaAvailable(mediaVisible());
    emit mediaChanged();
}

void MobilePreview::publishPosition(double second, bool forceRender)
{
    ++playbackSequence_;
    position_ = second;
    if (forceRender || !playing_ || canvasCadence_.due(presentationClock_.nsecsElapsed()))
        runtime_.setPlayheadSeconds(exportIntroEnabled_ && second < 0 ? 0 : second);
    emit positionChanged();
}

void MobilePreview::setPositionSeconds(double second)
{
    if (!qIsFinite(second)) return;
    second = qBound(lowerBoundSeconds(), second, duration_);
    if (exportIntroEnabled_ && second < 0) setPlaying(false);
    clockAnchor_ = second;
    clock_.restart();
    audioClockAge_.invalidate();
    audio_.setPosition(qMax<qint64>(0, qRound64(second * 1000)));
    video_.setPosition(qMax<qint64>(0, qRound64(second * 1000)));
    sfx_->seek(second, exportIntroEnabled_ && second < 0 ? 1 : rate_);
    publishPosition(second);
    emit playheadFocusRequested();
}

void MobilePreview::setRate(double rate)
{
    if (!qIsFinite(rate)) return;
    rate_ = qBound(0.25, rate, 2.0);
    audio_.setPlaybackRate(rate_);
    video_.setPlaybackRate(rate_);
    setPositionSeconds(position_);
    emit transportChanged();
}

void MobilePreview::setPlaying(bool playing)
{
    if (playing == playing_) return;
    if (playing && !sfx_->assetError().isEmpty()) { emit mediaError(sfx_->assetError()); return; }
    playing_ = playing;
    canvasCadence_.reset();
    videoFrames_.setPlaying(playing);
    ++playbackSequence_;
    if (playing) {
        if (rangeEnabled_ && (position_ < rangeStart_ || position_ >= rangeEnd_)) setPositionSeconds(rangeStart_);
        else if (position_ >= duration_) setPositionSeconds(lowerBoundSeconds());
        playbackEntry_ = position_;
        emit transportChanged();
        if (sfx_->ready()) startMediaPlayback();
        if (playing_) timer_.start();
    } else {
        timer_.stop();
        audio_.pause();
        video_.pause();
        sfx_->pause();
        runtime_.setPlayheadSeconds(exportIntroEnabled_ && position_ < 0 ? 0 : position_);
    }
    emit playingChanged();
    if (!playing_) emit playheadFocusRequested();
}

void MobilePreview::startMediaPlayback()
{
    sfx_->setEndSecond(exportIntroEnabled_ && position_ < 0 ? 0 : rangeEnabled_ ? rangeEnd_ : duration_);
    if (!playing_ || !sfx_->start(position_, exportIntroEnabled_ && position_ < 0 ? 1 : rate_)) return;
    clockAnchor_ = position_;
    clock_.restart(); audioClockAge_.invalidate();
    if (!audio_.source().isEmpty() && position_ >= 0) audio_.play();
    if (hasVideoMedia() && position_ >= 0 && mediaVisible()) video_.play();
}

void MobilePreview::tick()
{
    if (!sfx_->ready() || !sfx_->running()) return;
    const bool inIntro = exportIntroEnabled_ && position_ < 0;
    double second = clockAnchor_ + clock_.elapsed() / 1000.0 * (inIntro ? 1 : rate_);
    if (inIntro && second >= 0) {
        // v2 advances the intro at authoring speed, then starts chart transport
        // at zero using the selected chart rate and its own media clock.
        sfx_->pause();
        setPositionSeconds(0);
        startMediaPlayback();
        return;
    }
    if (hasVideoMedia() && second >= 0 && mediaVisible() && video_.playbackState() != QMediaPlayer::PlayingState) video_.play();
    if (!audio_.source().isEmpty() && second >= 0) {
        if (audio_.playbackState() != QMediaPlayer::PlayingState) audio_.play();
        if (audioClockAge_.isValid()) second = audioAnchor_ + qMin<qint64>(audioClockAge_.elapsed(), 150) / 1000.0 * rate_;
    }
    const double end = rangeEnabled_ ? rangeEnd_ : duration_;
    publishPosition(qMin(second, end), false);
    sfx_->synchronize(position_);
    if (second >= end) setPlaying(false);
}

void MobilePreview::setPlaybackRangeEnabled(bool enabled, double start, double end)
{
    if (!enabled || !qIsFinite(start) || !qIsFinite(end) || end <= start) {
        rangeEnabled_ = false;
        sfx_->setEndSecond(duration_);
        return;
    }
    rangeEnabled_ = true;
    rangeStart_ = qBound(lowerBoundSeconds(), start, duration_);
    rangeEnd_ = qBound(rangeStart_, end, duration_);
    if (rangeEnabled_ && rangeEnd_ <= rangeStart_) rangeEnabled_ = false;
    sfx_->setEndSecond(rangeEnabled_ ? rangeEnd_ : duration_);
    if (rangeEnabled_ && (position_ < rangeStart_ || position_ > rangeEnd_)) setPositionSeconds(rangeStart_);
}

void MobilePreview::stop() { setPlaying(false); setPositionSeconds(playbackEntry_); }
void MobilePreview::beginScrub() { if (scrubbing_) return; setPlaying(false); scrubbing_ = true; ++playbackSequence_; }
void MobilePreview::endScrub() { if (!scrubbing_) return; scrubbing_ = false; ++playbackSequence_; }

PlaybackSnapshot MobilePreview::playbackSnapshot() const
{
    const auto document = document_->workspace().snapshot();
    return {document.documentOpenGeneration, document.revision, playbackSequence_,
            position_, duration_, lowerBoundSeconds(), rate_,
            scrubbing_ ? PlaybackTransportState::Scrubbing : playing_ ? PlaybackTransportState::Playing
                : PlaybackTransportState::Paused};
}

QVariantList MobilePreview::statistics() const
{
    const auto s = statisticsCache_->snapshotAt(position_);
    QVariantList result;
    const auto row = [&result](const char* kind, const char* name, int played, int total) {
        result.append(QVariantMap{{"kind", kind}, {"name", name}, {"played", played}, {"total", total},
            {"value", QString::number(played) + "/" + QString::number(total)}, {"iconSource", QString()}});
    };
    row("tap", "Tap", s.tapPlayed, s.tapTotal);
    row("hold", "Hold", s.holdPlayed, s.holdTotal);
    row("slide", "Slide", s.slidePlayed, s.slideTotal);
    row("touch", "Touch", s.touchPlayed, s.touchTotal);
    row("break", "Break", s.breakPlayed, s.breakTotal);
    row("total", "Total", s.totalPlayed, s.totalCount);
    for (auto& value : result) {
        auto item = value.toMap();
        if (item.value("kind").toString() != "total") item.insert("iconSource", "image://noteicon/" + item.value("kind").toString());
        value = item;
    }
    return result;
}

QVariantMap MobilePreview::muriParameterRanges() const
{
    using namespace miacode::muri;
    return {{"handRadiusDefault", kHandRadiusDefaultPx}, {"handRadiusMin", kHandRadiusMinPx},
        {"handRadiusMax", kHandRadiusMaxPx}, {"handRadiusStep", kHandRadiusStepPx},
        {"tapOnSlideThresholdMin", kStaticTapOnSlideThresholdMinMs},
        {"tapOnSlideThresholdMax", kStaticTapOnSlideThresholdMaxMs},
        {"tapOnSlideThresholdStep", kStaticTapOnSlideThresholdStepMs}};
}
void MobilePreview::setMuriCheckEnabled(bool enabled)
{
    renderOptions_.renderMode = enabled ? RenderMode::MaimuriDxStyle : RenderMode::Native;
    refreshDocument();
    emit renderModeChanged();
}
void MobilePreview::setSmoothStarErase(bool enabled)
{
    renderOptions_.renderMode = enabled ? RenderMode::EraseByArea : RenderMode::Native;
    refreshDocument();
    emit renderModeChanged();
}
void MobilePreview::setMuriHandRadiusPx(int value)
{
    renderOptions_.handRadiusPx = miacode::muri::normalizedHandRadiusPx(value);
    refreshDocument();
    emit muriParametersChanged();
}
void MobilePreview::setMuriTapOnSlideThresholdMs(int value)
{
    thresholdMs_ = qBound(miacode::muri::kStaticTapOnSlideThresholdMinMs, value, miacode::muri::kStaticTapOnSlideThresholdMaxMs);
    refreshDocument();
    emit muriParametersChanged();
}

void MobilePreview::setHidePv(bool value)
{
    if (hidePv_ == value) return;
    hidePv_ = value;
    if (!mediaVisible()) video_.pause();
    else if (playing_ && position_ >= 0) video_.play();
    runtime_.setStageMediaAvailable(mediaVisible());
    emit mediaChanged();
}

void MobilePreview::setPausedJudgeAreaView(bool enabled)
{
    if (pausedJudgeAreaView_ == enabled) return;
    pausedJudgeAreaView_ = enabled;
    if (!mediaVisible()) video_.pause();
    else if (playing_ && position_ >= 0) video_.play();
    runtime_.setStageMediaAvailable(mediaVisible());
    emit mediaChanged();
}

void MobilePreview::setJudgeOverlayOptions(const MuriRenderOptions& options)
{
    // Preserve the independently selected render mode and analysis parameters.
    renderOptions_.showChartReviewSlideJudgeOverlay = options.showChartReviewSlideJudgeOverlay;
    renderOptions_.showChartReviewTapJudgeOverlay = options.showChartReviewTapJudgeOverlay;
    renderOptions_.showChartReviewBreakJudgeOverlay = options.showChartReviewBreakJudgeOverlay;
    renderOptions_.showChartReviewTouchJudgeOverlay = options.showChartReviewTouchJudgeOverlay;
    runtime_.setMuriRenderOptions(renderOptions_);
}

void MobilePreview::attachVideoOutputObject(QObject* output)
{
    attachVideoOutputObjects(output, nullptr);
}

void MobilePreview::clearVideoOutputs()
{
    videoFrames_.clear();
}

void MobilePreview::attachVideoOutputObjects(QObject* output, QObject* inner)
{
    if (!output) return;
    auto* sink = output->property("videoSink").value<QVideoSink*>();
    if (!sink) return;
    video_.setVideoSink(nullptr);
    innerVideoSink_.clear();
    videoSink_ = sink;
    if (inner) {
        auto* innerSink = inner->property("videoSink").value<QVideoSink*>();
        if (innerSink && innerSink != sink) {
            innerVideoSink_ = innerSink;
        }
    }
    videoFrames_.attach(videoSink_, innerVideoSink_);
    auto* quickOutput = qobject_cast<QQuickItem*>(output);
    videoFrames_.watchWindow(quickOutput ? quickOutput->window() : nullptr);
    video_.setVideoSink(videoFrames_.input());
}

void MobilePreview::detachVideoOutputObject(QObject* output)
{
    if (!output || output->property("videoSink").value<QVideoSink*>() != videoSink_) return;
    video_.setVideoSink(nullptr);
    videoFrames_.detach();
    videoSink_.clear();
    innerVideoSink_.clear();
}
}
