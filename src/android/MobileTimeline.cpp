#include "MobileTimeline.h"
#include "common/AssetPaths.h"
#include "core/chart/document/SimaiTimingMetadata.h"
#include "timeline/TimelineMarkerOffset.h"
#include "app/ui/preferences/LocaleService.h"
#include "app/ui/preferences/PreferenceDocument.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QPointer>

namespace miacode::android {
namespace {
void saveTimelinePreference(const char* key, const QJsonValue& value)
{
    auto root = PreferenceDocument::loadPreferencesObject();
    auto app = root.value("app").toObject();
    auto preferences = app.value("preview").toObject();
    preferences.insert(QLatin1String(key), value);
    app.insert("preview", preferences);
    root.insert("app", app);
    PreferenceDocument::savePreferencesObject(root);
}
}
MobileTimeline::MobileTimeline(AndroidDocumentSession& document, MobilePreview& preview,
    EditorSyncController& editor, AnalysisService& analysis, QObject* parent)
    : QObject(parent), document_(document), preview_(preview), editor_(editor),
      bridge_(this), waveforms_(this)
{
    bridge_.setSkinDirectory(assets::assetPath("skin/skinDX"));
    const auto preferences = PreferenceDocument::loadPreferencesObject().value("app").toObject().value("preview").toObject();
    if (preferences.value("timeline_zoom_scale").isDouble())
        bridge_.setZoomScale(preferences.value("timeline_zoom_scale").toDouble());
    if (preferences.value("timeline_waveform_brightness").isDouble())
        bridge_.setWaveformBrightness(preferences.value("timeline_waveform_brightness").toDouble());
    if (preferences.value("timeline_measure_line_brightness").isDouble())
        bridge_.setMeasureLineBrightness(preferences.value("timeline_measure_line_brightness").toDouble());
    if (preferences.value("follow_preview").isBool())
        bridge_.setFollowPreviewEnabled(preferences.value("follow_preview").toBool());
    bridge_.setViewportLockEnabled(true);
    // Restore before connecting persistence so startup does not rewrite the
    // user's document. The shared bridge owns normalization and zoom presets.
    connect(&bridge_, &TimelineQuickStateBridge::zoomScaleChanged, this, [](double value) {
        saveTimelinePreference("timeline_zoom_scale", value);
    });
    connect(&bridge_, &TimelineQuickStateBridge::waveformBrightnessChanged, this, [](double value) {
        saveTimelinePreference("timeline_waveform_brightness", value);
    });
    connect(&bridge_, &TimelineQuickStateBridge::measureLineBrightnessChanged, this, [](double value) {
        saveTimelinePreference("timeline_measure_line_brightness", value);
    });
    connect(&bridge_, &TimelineQuickStateBridge::followPreviewEnabledChanged, this, [](bool value) {
        saveTimelinePreference("follow_preview", value);
    });
    connect(&document_, &AndroidDocumentSession::documentStateChanged, this, &MobileTimeline::rebuild);
    connect(&document_, &AndroidDocumentSession::mediaAssetsChanged, this, &MobileTimeline::rebuild);
    connect(&preview_, &MobilePreview::previewSourceChanged, this, &MobileTimeline::rebuild);
    presentationClock_.start();
    presentationTimer_.setTimerType(Qt::PreciseTimer);
    presentationTimer_.setInterval(cadence_.pollingIntervalMs());
    connect(&presentationTimer_, &QTimer::timeout, this, [this] {
        if (cadence_.due(presentationClock_.nsecsElapsed())) publishPosition();
    });
    connect(&preview_, &MobilePreview::positionChanged, this, [this] {
        if (!preview_.playing() || navigating_) publishPosition();
    });
    connect(&preview_, &MobilePreview::playheadFocusRequested, this, [this] {
        if (!navigating_) bridge_.setPlayheadSeconds(preview_.positionSeconds(), true);
    });
    connect(&LocaleService::instance(), &LocaleService::languageChanged, this, &MobileTimeline::labelsChanged);
    connect(&preview_, &MobilePreview::transportChanged, this, [this] {
        bridge_.setPlayheadUpperLimitSeconds(preview_.durationSeconds());
        bridge_.setPlaybackEntrySeconds(preview_.playbackEntrySeconds());
    });
    connect(&preview_, &MobilePreview::playingChanged, this, [this] {
        cadence_.reset();
        if (preview_.playing()) presentationTimer_.start();
        else presentationTimer_.stop();
        bridge_.setPlaybackCadenceActive(preview_.playing());
        editor_.setPlaybackActive(preview_.playing());
        publishPosition();
    });
    connect(&editor_, &EditorSyncController::caretLocationPublished, this,
        [this](int difficulty, quint64 revision, int line, int column) {
            if (preview_.latencyChartActive() || navigating_ || difficulty != document_.activeDifficulty()
                || revision != document_.documentRevision()) return;
            double second = 0;
            if (model_.resolveTimelineSecondForCursor(line, column, &second))
                bridge_.setCursorSeconds(second, !preview_.playing());
        });
    connect(&editor_, &EditorSyncController::previewSeekPublished, this,
        [this](int difficulty, int line, int column) {
            double second = 0;
            if (!preview_.latencyChartActive() && difficulty == document_.activeDifficulty()
                && model_.resolveTimelineSecondForCursor(line, column, &second))
                preview_.setPositionSeconds(second);
        });
    connect(&analysis, &AnalysisService::analysisReady, this,
        [this, &analysis](int difficulty, quint64 revision) {
            if (preview_.latencyChartActive() || difficulty != document_.activeDifficulty() || revision != document_.documentRevision()) return;
            const auto result = analysis.snapshot();
            bridge_.setMuriAnalysisReport(result.muri);
            preview_.sceneRuntime().setMuriAnalysisReport(result.muri);
        });
    rebuild();
}

QString MobileTimeline::timelineTabLabel() const { return qtTrId("window.timeline"); }
QString MobileTimeline::validationTabLabel() const { return qtTrId("window.syntax"); }
QString MobileTimeline::muriTabLabel() const { return qtTrId("window.muri"); }
QString MobileTimeline::followCodeLabel() const { return qtTrId("shell.follow_code"); }
void MobileTimeline::setFrameRate(PreviewCanvasFrameRateMode mode, double refreshHz) {
    cadence_.configure(mode, refreshHz);
    presentationTimer_.setInterval(cadence_.pollingIntervalMs());
    publishPosition();
}
void MobileTimeline::setCurrentTabId(const QString& tab) {
    if (tab == tab_ || (tab != "timeline" && tab != "validation" && tab != "muri")) return;
    tab_ = tab;
    emit tabChanged();
}

void MobileTimeline::rebuild() {
    const auto& data = document_.workspace().document();
    model_.rebuildFromText(preview_.previewChartText(), preview_.previewChartOffset(),
        preview_.latencyChartActive() ? simai::SimaiTimingMetadata() : simai::buildTimingMetadata(data));
    bridge_.setTimelineData(model_.snapshot());
    bridge_.setPlaybackEntrySeconds(preview_.playbackEntrySeconds());
    bridge_.setPlayheadUpperLimitSeconds(preview_.durationSeconds());
    const QFileInfo track(document_.previewAssetPath("audio"));
    const QString identity = track.absoluteFilePath() + ':' + QString::number(track.size())
        + ':' + QString::number(track.lastModified().toMSecsSinceEpoch());
    if (identity != trackIdentity_) {
        trackIdentity_ = identity;
        const quint64 request = ++waveformRequest_;
        bridge_.setWaveformData({});
        preview_.setDecodedTrackDuration(track.absoluteFilePath(), 0);
        if (track.isFile()) {
            QPointer<MobileTimeline> guard(this);
            // Cache lives beside the private project copy, never in the SAF provider.
            waveforms_.requestWaveform(track.absoluteFilePath(), track.absolutePath() + "/.waveform-cache",
                [guard, request, path = track.absoluteFilePath()](waveform::WaveformDataPtr waveform) {
                    if (guard && guard->waveformRequest_ == request) {
                        guard->bridge_.setWaveformData(waveform);
                        guard->preview_.setDecodedTrackDuration(path, waveform ? waveform->durationSeconds : 0);
                    }
                });
        }
    }
    publishPosition();
}

void MobileTimeline::publishPosition() {
    bridge_.setPlayheadSeconds(preview_.positionSeconds(), preview_.playing() && bridge_.followProgressEnabled());
    EditorFollowState follow;
    follow.difficultyId = document_.activeDifficulty();
    follow.revision = document_.documentRevision();
    follow.playbackActive = preview_.playing();
    TimelineQuickModel::PreviewFollowSpan span;
    if (!preview_.latencyChartActive() && model_.resolvePreviewFollowSpan(preview_.positionSeconds(), &span)) {
        follow.active = true;
        follow.start = span.startPosition;
        follow.end = span.endPositionExclusive;
        follow.caret = span.cursorPosition;
        follow.reveal = bridge_.followPreviewEnabled();
    }
    editor_.publishFollow(follow);
}

void MobileTimeline::navigate(double second, bool center) {
    if (!qIsFinite(second)) return;
    navigating_ = true;
    preview_.setPositionSeconds(second);
    int line = 1, column = 1;
    double cursorSecond = 0;
    if (!preview_.latencyChartActive() && model_.resolveTimelineNavigateCursor(second, &line, &column, &cursorSecond)) {
        const int position = document_.chartPosition(line, column);
        editor_.requestNavigation(document_.activeDifficulty(), document_.documentRevision(), position, position, false, true);
        bridge_.setCursorSeconds(cursorSecond, center);
    }
    bridge_.setPlayheadSeconds(preview_.positionSeconds(), center);
    navigating_ = false;
}
void MobileTimeline::headerNavigate(double second) { navigate(second, false); }
void MobileTimeline::wheelNavigate(double second) { navigate(second, false); }
void MobileTimeline::centerNavigate(double second) { navigate(second, true); }
void MobileTimeline::dragStarted() { preview_.beginScrub(); }
void MobileTimeline::dragFinished(double second) { navigate(second, false); preview_.endScrub(); }
void MobileTimeline::userInteractionStarted() { bridge_.setFollowProgressEnabled(false); }
void MobileTimeline::surfaceReady() { publishPosition(); }
void MobileTimeline::followPreviewToggled(bool enabled) { bridge_.setFollowPreviewEnabled(enabled); publishPosition(); }
}
