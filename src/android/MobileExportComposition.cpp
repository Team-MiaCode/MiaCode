#include "MobileExportComposition.h"
#include "ExportDestination.h"
#include "MobileBatchExport.h"
#include "common/AssetPaths.h"
#include "common/ChartAssetPaths.h"
#include "common/IntroConfig.h"
#include "common/ChartClockCount.h"
#include "tools/video_export/FontLibrary.h"
#include "common/ProjectPreferences.h"
#include "tools/video_export/VideoExportSettings.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QScopedValueRollback>
#include <QEventLoop>
#include <QGuiApplication>
#include <QScopeGuard>

namespace miacode::android {
MobileExportComposition::MobileExportComposition(AndroidDocumentSession& document, MobilePreview& preview,
    MobileVideoExport& exporter, QObject* parent)
    : QObject(parent), document_(document), preview_(preview), exporter_(exporter),
      session_(notifications_, requests_, progress_, appearance_, engineSlot_, surfaceSlot_),
      settings_(notifications_, requests_, appearance_, surfaceSlot_),
      audioAudition_(preview, this),
      audioSettingsModel_(surfaceSlot_, {
          [this](const QString& kind, const QString& directory, const PreviewAudioSettings& levels) {
              return audioAudition_.play(kind, directory, levels);
          }, [this] { audioAudition_.release(); }}) {
    connect(&audioAudition_, &MobileAudioAudition::failed, this, [this](const QString& message) {
        requests_.postNotice(NoticeSeverity::Error, qtTrId("action.audio_settings"), message);
    });
    connect(&preview_, &MobilePreview::mediaError, this, [this](const QString& message) {
        requests_.postNotice(NoticeSeverity::Error, qtTrId("action.audio_settings"), message);
    });
    appearance_.setSkinDirectory(QFileInfo(resolveSkinDir()).fileName());
    connect(&appearance_, &PreviewAppearanceState::changed, this, [this] {
        refreshSurfaces(); savePreviewPreferences();
    });
    connect(&preview_, &MobilePreview::playingChanged, this, &MobileExportComposition::applyEffectiveOutline);
    connect(&preview_, &MobilePreview::positionChanged, this, &MobileExportComposition::updateIntroFrame);
    connect(&session_, &ui::ExportSession::introChanged, this, &MobileExportComposition::refreshIntroState);
    connect(&session_, &ui::ExportSession::videoChanged, this, &MobileExportComposition::refreshIntroState);
    connect(&session_, &ui::ExportSession::rangeChanged, this, &MobileExportComposition::refreshIntroState);
    connect(&session_, &ui::ExportSession::pageSessionActiveChanged, this, &MobileExportComposition::applyEffectiveOutline);
    connect(&document_, &AndroidDocumentSession::documentReplaced, this, [this] {
        loadProjectAudioPreferences();
        previousDifficulty_ = 0;
        session_.replaceDocument(document_.activeDifficulty());
    });
    connect(&exporter_, &MobileVideoExport::changed, this, [this] {
        emit notifications_.videoExportWorkerRunningChanged(exporter_.running());
        if (!batchRunning_ && exporter_.running() && progress_.token() == jobToken_)
            progress_.report(exporter_.percent(), exportDestinationDisplayPath(exporter_.outputPath()));
    });
    connect(&exporter_, &MobileVideoExport::finished, this, [this](bool ok, const QString& output, const QString& error) {
        if (batchRunning_) return;
        if (progress_.token() == jobToken_) progress_.end();
        requests_.postNotice(ok ? NoticeSeverity::Information : NoticeSeverity::Error,
            qtTrId("dialog.video_export.title"), ok ? exportDestinationDisplayPath(output) : error);
    });
    connect(&progress_, &JobProgressService::cancellationRequested, this, [this](quint64 token) {
        if (token == jobToken_) exporter_.cancel();
    });
    restorePreviewPreferences();
    loadProjectAudioPreferences();
    restoringPreferences_ = false;
    applyEffectiveOutline();
}
MobileExportComposition::~MobileExportComposition() {
    audioSettingsModel_.releaseAudition();
    session_.leave();
    engineSlot_ = nullptr; surfaceSlot_ = nullptr;
}
VideoExportTask MobileExportComposition::buildSeedTask(int id) {
    auto task = exporter_.buildTask(id);
    const auto& state = preview_.sceneRuntime().frameState();
    const auto& r = state.render;
    task.audioSettings = audioSettings_;
    task.backgroundBrightnessOuter = r.backgroundBrightnessOuter;
    task.backgroundBrightnessInner = r.backgroundBrightnessInner;
    task.layoutSquareScale = r.layoutSquareScale; task.backgroundScaleMode = r.backgroundScaleMode;
    task.smoothBrightness = r.smoothBrightness; task.tapFlowSpeed = r.tapFlowSpeed; task.touchFlowSpeed = r.touchFlowSpeed;
    task.showTimestamp = r.showTimestamp; task.showObjectStatsHud = r.showObjectStatsHud;
    task.showChartInfoHud = r.showChartInfoHud; task.fixHudTextLayout = r.fixHudTextLayout;
    task.outlineVariant = appearance_.outlineVariant();
    task.outlineImagePath = customOutline_.isEmpty() ? QString() : QDir(resolveCustomOutlineDir()).filePath(customOutline_);
    task.judgeEffectStyle = appearance_.judgeEffectStyle(); task.tapJudgeTextDistance = appearance_.tapJudgeTextDistance();
    task.slideEarlierSecondAndTextOnTop = appearance_.slideEarlierSecondAndTextOnTop();
    task.centerDisplayMode = appearance_.centerDisplayMode();
    task.muriAnalysisReport = state.muriAnalysisReport;
    return task;
}
void MobileExportComposition::applySharedTaskSettings(const VideoExportTask& task) {
    if (previousDifficulty_ || session_.pageSessionActive())
        preview_.setCanvasAspectRatio(task.outputWidth > 0 && task.outputHeight > 0
            ? static_cast<double>(task.outputWidth) / task.outputHeight : 1.0);
    auto& runtime = preview_.sceneRuntime();
    runtime.setBackgroundBrightnessOuter(task.backgroundBrightnessOuter);
    runtime.setBackgroundBrightnessInner(task.backgroundBrightnessInner);
    runtime.setLayoutSquareScale(task.layoutSquareScale); runtime.setBackgroundScaleMode(task.backgroundScaleMode);
    runtime.setSmoothBrightness(task.smoothBrightness); runtime.setTapFlowSpeed(task.tapFlowSpeed);
    runtime.setTouchFlowSpeed(task.touchFlowSpeed); runtime.setShowTimestamp(task.showTimestamp);
    runtime.setShowObjectStatsHud(task.showObjectStatsHud); runtime.setShowChartInfoHud(task.showChartInfoHud);
    runtime.setFixHudTextLayout(task.fixHudTextLayout);
    preview_.refreshMediaLayout();
    savePreviewPreferences();
    emit notifications_.previewRenderSettingsChanged();
}
bool MobileExportComposition::startAudition(int id, const VideoExportTask& task) {
    if (!document_.workspace().document().difficulty(id)) return false;
    if (!previousDifficulty_) previousDifficulty_ = document_.activeDifficulty();
    preview_.setPlaying(false); document_.selectDifficulty(id); applySharedTaskSettings(task);
    refreshIntroState();
    return true;
}
void MobileExportComposition::stopAudition() {
    preview_.setPlaying(false);
    preview_.setCanvasAspectRatio(1.0);
    preview_.configureExportAudition(false, false, 0, 0);
    preview_.sceneRuntime().clearIntroOverlay();
    if (!previousDifficulty_) return;
    const int id = previousDifficulty_; previousDifficulty_ = 0;
    if (document_.workspace().document().difficulty(id)) document_.selectDifficulty(id);
}
bool MobileExportComposition::launchVideoExport(const VideoExportTask& requested, int id, QString* error) {
    if (batchRunning_) { if (error) *error = QStringLiteral("A batch export is already running"); return false; }
    auto task = buildSeedTask(id);
    video_export::copyVideoExportUserSettings(requested, &task);
    // The range is a command, not a persisted preference.
    task.exportStartSeconds = requested.exportStartSeconds;
    task.contentDurationSeconds = requested.contentDurationSeconds;
    task.fullRangeExport = requested.fullRangeExport;
    task.outputPath = requested.outputPath;
    jobToken_ = progress_.begin(qtTrId("dialog.video_export.title"), exportDestinationDisplayPath(task.outputPath), true, JobProgressService::TaskType::ChartExport);
    if (!exporter_.start(task, error)) { progress_.end(); return false; }
    return true;
}
bool MobileExportComposition::launchBatchExport(const VideoExportTask& settings, const QStringList& directories,
    const QList<int>& ids, const QString& outputDirectory, BatchResult* result, const BatchCallbacks& callbacks, QString* error) {
    const auto reject = [error](const QString& message) { if (error) *error = message; return false; };
    if (error) error->clear();
    if (!result) return reject(QStringLiteral("Batch result is missing"));
    *result = {};
    if (batchRunning_ || exporter_.running()) return reject(QStringLiteral("An export is already running"));
    if (directories.isEmpty()) return reject(qtTrId("dialog.batch_export.error.no_chart_dirs"));
    if (ids.isEmpty()) return reject(qtTrId("dialog.batch_export.error.no_difficulties"));
    if (outputDirectory.trimmed().isEmpty()) return reject(qtTrId("dialog.batch_export.error.no_output_dir"));
    if (!QDir().mkpath(outputDirectory)) return reject(qtTrId("dialog.batch_export.error.output_dir_create_failed"));
    const auto capturedDirectories = directories;
    const auto capturedIds = ids;
    const auto capturedOutput = outputDirectory;
    const auto capturedSettings = settings;
    const QScopedValueRollback batchGuard(batchRunning_, true);
    if (!exporter_.beginBatchExecution(error)) return false;
    const auto batchLifetime = qScopeGuard([&] { exporter_.endBatchExecution(); });
    std::atomic_bool canceled{false};
    const auto quitting = connect(qGuiApp, &QCoreApplication::aboutToQuit, this, [&] {
        canceled.store(true);
        exporter_.cancel();
    });
    const auto quitLifetime = qScopeGuard([&] { disconnect(quitting); });
    const auto pollCancellation = [&] {
        if (exporter_.batchCancellationRequested()
            || (callbacks.cancellationRequested && callbacks.cancellationRequested())) canceled.store(true);
#ifdef Q_OS_ANDROID
        if (!document_.backgroundExportAllowed() && qGuiApp->applicationState() != Qt::ApplicationActive) canceled.store(true);
#endif
    };
    exporter_.updateBatchProgress(0);
    if (callbacks.progressChanged) callbacks.progressChanged(0, qtTrId("export.preparing_package"));
    pollCancellation();
    MobileBatchExportPlan plan;
    QEventLoop preparation;
    QTimer cancellationTimer;
    cancellationTimer.setInterval(50);
    connect(&cancellationTimer, &QTimer::timeout, &preparation, pollCancellation);
    QString preparationFailure;
    // Copy every UI-owned argument before pumping events.
    std::thread worker([&] {
        try { plan = prepareMobileBatchExport(capturedDirectories, capturedIds, capturedOutput, capturedSettings, canceled); }
        catch (const std::exception& failure) { preparationFailure = QString::fromUtf8(failure.what()); }
        QMetaObject::invokeMethod(&preparation, &QEventLoop::quit, Qt::QueuedConnection);
    });
    cancellationTimer.start(); preparation.exec(); canceled.store(canceled.load() || qGuiApp->closingDown());
    worker.join(); cancellationTimer.stop();
    pollCancellation();
    if (!preparationFailure.isEmpty()) return reject(preparationFailure);
    result->failedCharts = plan.failures;
    result->canceled = plan.canceled || canceled.load();
    for (int index = 0; index < plan.jobs.size() && !result->canceled; ++index) {
        pollCancellation();
        if (canceled.load()) { result->canceled = true; break; }
        const auto& job = plan.jobs.at(index);
        QEventLoop completion;
        QTimer heartbeat;
        heartbeat.setInterval(50);
        bool finished = false, successful = false, reporting = false;
        QString failure, output;
        const auto connection = connect(&exporter_, &MobileVideoExport::finished, &completion,
            [&](bool ok, const QString& path, const QString& message) {
                finished = true; successful = ok; failure = message; output = path; completion.quit();
            });
        connect(&heartbeat, &QTimer::timeout, &completion, [&] {
            if (reporting) return;
            const QScopedValueRollback reportingGuard(reporting, true);
            pollCancellation();
            if (canceled.load()) exporter_.cancel();
            const int percent = qBound(0, qRound(100.0 * (index + exporter_.percent() / 100.0) / plan.jobs.size()), 100);
            exporter_.updateBatchProgress(percent);
            if (callbacks.progressChanged) callbacks.progressChanged(percent,
                qtTrId("dialog.batch_export.progress.exporting_named").arg(index + 1).arg(plan.jobs.size()).arg(job.displayName));
        });
        if (!exporter_.start(job.task, &failure, true)) {
            disconnect(connection); result->failedCharts.append(job.displayName + " - " + failure); continue;
        }
        heartbeat.start();
        if (!finished) completion.exec();
        heartbeat.stop(); disconnect(connection);
        pollCancellation();
        if (canceled.load() || exporter_.cancellationRequested()) { result->canceled = true; break; }
        if (!finished || !successful) { result->failedCharts.append(job.displayName + " - " + failure); continue; }
        ++result->successCount;
        result->exportedFiles.append(exportDestinationDisplayPath(output));
    }
    if (callbacks.progressChanged && !result->canceled) callbacks.progressChanged(100, QString());
    return true;
}
QString MobileExportComposition::difficultyChartText(int id) const {
    const auto* difficulty = document_.workspace().document().difficulty(id);
    return difficulty ? difficulty->chart : QString();
}
void MobileExportComposition::refreshIntroState() {
    if (!previousDifficulty_ || !session_.pageSessionActive()) return;
    const auto spec = session_.previewIntroSpec();
    const auto& document = document_.workspace().document();
    preview_.configureExportAudition(true, spec.enabled,
        session_.fullRangeExport() && session_.clockCountEnabled() ? chart_clock::clockCountFromDocument(document) : 0,
        chart_clock::clockBpmForChart(document, document_.chartText()));
    if (spec.enabled) preview_.setIntroSoundFile(session_.introSoundFileName());
    auto& runtime = preview_.sceneRuntime();
    if (!spec.enabled) { runtime.clearIntroOverlay(); return; }
    QFile file(QStringLiteral(":/intro/templates/maimai_banner.json"));
    if (!file.open(QIODevice::ReadOnly)) return;
    auto banner = QJsonDocument::fromJson(file.readAll()).object().toVariantMap();
    video_export::applyBannerFontOverride(banner, spec.fontDisplayPath, spec.fontBodyPath);
    runtime.setIntroOverlayData(introBannerTrackMap(spec), banner,
        chart_assets::displayBackgroundImageUrl(spec.jacketPath), QUrl(QString::fromLatin1(intro::kLogoFallbackUrl)), introBannerStyleMap(spec));
    if (!preview_.playing() && qAbs(preview_.positionSeconds()) <= 0.05)
        preview_.setPositionSeconds(-intro::kDurationSeconds);
    updateIntroFrame();
}
void MobileExportComposition::updateIntroFrame() {
    auto& runtime = preview_.sceneRuntime();
    if (!previousDifficulty_ || !session_.pageSessionActive() || !session_.previewIntroSpec().enabled
        || preview_.positionSeconds() >= 0) {
        if (runtime.introOverlayActive()) runtime.clearIntroOverlay(true);
        return;
    }
    const int frame = qBound(0, qRound((preview_.positionSeconds() + intro::kDurationSeconds)
        * intro::kAuthoringFps), intro::kDurationFrames);
    runtime.setIntroOverlayFrame(frame, true);
}
PlaybackTransportState MobileExportComposition::playbackTransportState() const {
    return preview_.playing() ? PlaybackTransportState::Playing : PlaybackTransportState::Paused;
}
QStringList MobileExportComposition::statsTexts() const {
    QStringList values;
    for (const auto& item : preview_.statistics()) values.append(item.toMap().value("value").toString());
    return values;
}
void MobileExportComposition::setMuriRenderMode(RenderMode mode) {
    if (mode == RenderMode::MaimuriDxStyle) preview_.setMuriCheckEnabled(true);
    else preview_.setSmoothStarErase(mode == RenderMode::EraseByArea);
}
QString MobileExportComposition::resolveSkinRootDir() const { return assets::assetPath("skin"); }
QStringList MobileExportComposition::availableSkinDirectoryNames() const {
    const QDir root(resolveSkinRootDir());
    const auto hasCoreAssets = [&root](const QString& name) {
        const QDir skin(root.filePath(name));
        return QFileInfo::exists(skin.filePath("tap.png")) && QFileInfo::exists(skin.filePath("hold.png"))
            && QFileInfo::exists(skin.filePath("star.png"));
    };
    QStringList names;
    const QStringList builtIns{QStringLiteral("skinSD"), QStringLiteral("skinDX")};
    for (const auto& name : builtIns) if (hasCoreAssets(name)) names.append(name);
    for (const auto& name : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase)) {
        bool builtIn = false;
        for (const auto& candidate : builtIns) builtIn |= name.compare(candidate, Qt::CaseInsensitive) == 0;
        if (!builtIn && hasCoreAssets(name)) names.append(name);
    }
    return names;
}
QString MobileExportComposition::skinDisplayName(const QString& name) const {
    const auto trimmed = name.trimmed();
    if (trimmed.compare("skinSD", Qt::CaseInsensitive) == 0 || trimmed.compare("skinSTD", Qt::CaseInsensitive) == 0)
        return qtTrId("dialog.render_settings.video.skin.standard");
    if (trimmed.compare("skinDX", Qt::CaseInsensitive) == 0) return qtTrId("dialog.render_settings.video.skin.dx");
    return trimmed;
}
QString MobileExportComposition::resolveCustomOutlineDir() const { return assets::customOutlineRootPath(); }
QStringList MobileExportComposition::availableCustomOutlineFileNames() const {
    return QDir(resolveCustomOutlineDir()).entryList({"*.png"}, QDir::Files, QDir::Name | QDir::IgnoreCase);
}
void MobileExportComposition::applyOutlineVariant(PreviewOutlineVariant value, bool, bool persist) {
    QScopedValueRollback guard(restoringPreferences_, restoringPreferences_ || !persist);
    value = static_cast<PreviewOutlineVariant>(qBound(0, static_cast<int>(value), 3));
    customOutline_.clear(); appearance_.setOutlineVariant(value);
    applyEffectiveOutline();
    savePreviewPreferences();
    emit notifications_.previewRenderSettingsChanged();
}
void MobileExportComposition::applyCustomOutlineFileName(const QString& name, bool persist) {
    if (!availableCustomOutlineFileNames().contains(name)) return;
    QScopedValueRollback guard(restoringPreferences_, restoringPreferences_ || !persist);
    customOutline_ = name;
    applyEffectiveOutline();
    savePreviewPreferences();
    emit notifications_.previewRenderSettingsChanged();
}
QVariantMap MobileExportComposition::renderSettings() const {
    const auto& r = preview_.sceneRuntime().frameState().render;
    QVariantMap values;
    values.insert("brightnessOuter", r.backgroundBrightnessOuter * 100); values.insert("brightnessInner", r.backgroundBrightnessInner * 100);
    values.insert("layoutSquareScale", r.layoutSquareScale * 100);
    values.insert("layoutSquareScaleMin", preview_video::kLayoutSquareScaleMin * 100);
    values.insert("layoutSquareScaleMax", preview_video::kLayoutSquareScaleMax * 100);
    values.insert("layoutSquareScaleStep", preview_video::kLayoutSquareScaleStep * 100);
    values.insert("scaleMode", static_cast<int>(r.backgroundScaleMode)); values.insert("smoothBrightness", r.smoothBrightness);
    values.insert("showTimestamp", r.showTimestamp); values.insert("showDebugInfo", r.showDebugInfo);
    values.insert("tapFlowSpeed", r.tapFlowSpeed); values.insert("touchFlowSpeed", r.touchFlowSpeed);
    values.insert("flowSpeedMin", preview_gameplay::kPreviewTimingFlowSpeedMin);
    values.insert("flowSpeedMax", preview_gameplay::kPreviewTimingFlowSpeedMax);
    values.insert("flowSpeedStep", preview_gameplay::kPreviewTimingFlowSpeedStep);
    const auto& options = preview_.muriRenderOptions();
    values.insert("judgeEffectSlide", options.showChartReviewSlideJudgeOverlay);
    values.insert("judgeEffectTap", options.showChartReviewTapJudgeOverlay);
    values.insert("judgeEffectBreak", options.showChartReviewBreakJudgeOverlay);
    values.insert("judgeEffectTouch", options.showChartReviewTouchJudgeOverlay);
    values.insert("forceLabeledJudgeLineWhenPaused", forceLabeledJudgeLineWhenPaused_);
    values.insert("slideEarlierOnTop", appearance_.slideEarlierSecondAndTextOnTop());
    values.insert("centerDisplay", static_cast<int>(appearance_.centerDisplayMode()));
    values.insert("tapJudgeTextDistance", static_cast<int>(appearance_.tapJudgeTextDistance()));
    values.insert("touchPadAuthoringShortcut", preview_.sceneRuntime().touchPadAuthoringEnabled());
    return values;
}
void MobileExportComposition::setRenderSetting(const QString& key, const QVariant& value) {
    auto& runtime = preview_.sceneRuntime();
    if (key == "brightnessOuter") runtime.setBackgroundBrightnessOuter(qRound(qBound(0.0, value.toDouble(), 100.0)) / 100.0);
    else if (key == "brightnessInner") runtime.setBackgroundBrightnessInner(qRound(qBound(0.0, value.toDouble(), 100.0)) / 100.0);
    else if (key == "layoutSquareScale") runtime.setLayoutSquareScale(value.toDouble() / 100);
    else if (key == "scaleMode") runtime.setBackgroundScaleMode(static_cast<PreviewBackgroundScaleMode>(qBound(0, value.toInt(), 3)));
    else if (key == "smoothBrightness") runtime.setSmoothBrightness(value.toBool());
    else if (key == "showTimestamp") runtime.setShowTimestamp(value.toBool());
    else if (key == "showDebugInfo") runtime.setShowDebugInfo(value.toBool());
    else if (key == "showObjectStatsHud") runtime.setShowObjectStatsHud(value.toBool());
    else if (key == "showChartInfoHud") runtime.setShowChartInfoHud(value.toBool());
    else if (key == "fixHudTextLayout") runtime.setFixHudTextLayout(value.toBool());
    else if (key == "touchPadAuthoringShortcut") runtime.setTouchPadAuthoringEnabled(value.toBool());
    else if (key == "tapFlowSpeed" || key == "touchFlowSpeed") {
        const double speed = preview_gameplay::normalizePreviewTimingFlowSpeed(value.toDouble());
        const double steps = qRound((speed - preview_gameplay::kPreviewTimingFlowSpeedMin) / preview_gameplay::kPreviewTimingFlowSpeedStep);
        const double snapped = preview_gameplay::kPreviewTimingFlowSpeedMin + steps * preview_gameplay::kPreviewTimingFlowSpeedStep;
        if (key == "tapFlowSpeed") runtime.setTapFlowSpeed(snapped);
        else runtime.setTouchFlowSpeed(snapped);
    }
    else if (key == "forceLabeledJudgeLineWhenPaused") {
        forceLabeledJudgeLineWhenPaused_ = value.toBool(); applyEffectiveOutline();
    }
    else if (key == "judgeEffectSlide" || key == "judgeEffectTap" || key == "judgeEffectBreak" || key == "judgeEffectTouch") {
        auto options = preview_.muriRenderOptions();
        if (key == "judgeEffectSlide") options.showChartReviewSlideJudgeOverlay = value.toBool();
        else if (key == "judgeEffectTap") options.showChartReviewTapJudgeOverlay = value.toBool();
        else if (key == "judgeEffectBreak") options.showChartReviewBreakJudgeOverlay = value.toBool();
        else options.showChartReviewTouchJudgeOverlay = value.toBool();
        preview_.setJudgeOverlayOptions(options);
    }
    else if (key == "slideEarlierOnTop") appearance_.setSlideEarlierSecondAndTextOnTop(value.toBool());
    else if (key == "centerDisplay") appearance_.setCenterDisplayMode(static_cast<preview_gameplay::CenterDisplayMode>(qBound(0, value.toInt(), 7)));
    else if (key == "tapJudgeTextDistance") appearance_.setTapJudgeTextDistance(static_cast<PreviewTapJudgeTextDistance>(qBound(0, value.toInt(), 2)));
    else return; // Unknown keys cannot pretend to change the live renderer.
    if (key == "scaleMode" || key == "layoutSquareScale") preview_.refreshMediaLayout();
    savePreviewPreferences();
    emit notifications_.previewRenderSettingsChanged();
}
void MobileExportComposition::refreshSurfaces() {
    auto& runtime = preview_.sceneRuntime();
    runtime.setSkinDirectory(QDir(resolveSkinRootDir()).filePath(appearance_.skinDirectoryName()));
    runtime.setJudgeEffectStyle(appearance_.judgeEffectStyle()); applyEffectiveOutline();
    runtime.setSlideEarlierSecondAndTextOnTop(appearance_.slideEarlierSecondAndTextOnTop());
    runtime.setTapJudgeTextDistance(appearance_.tapJudgeTextDistance()); runtime.setCenterDisplayMode(appearance_.centerDisplayMode());
    emit notifications_.previewSkinDirectoryChanged(); emit notifications_.previewRenderSettingsChanged();
}
void MobileExportComposition::applyEffectiveOutline() {
    const bool pausedJudgeArea = forceLabeledJudgeLineWhenPaused_ && !preview_.playing() && !session_.pageSessionActive();
    const auto variant = pausedJudgeArea ? PreviewOutlineVariant::JudgeAreaLabeled : appearance_.outlineVariant();
    const auto path = customOutline_.isEmpty() ? QString() : assets::customOutlinePathForFileName(customOutline_);
    const auto mode = pausedJudgeArea && !path.isEmpty()
        ? preview::runtime::PreviewOutlineImageMode::PausedJudgeAreaComposite : preview::runtime::PreviewOutlineImageMode::Direct;
    preview_.sceneRuntime().setOutlineSelection(variant, path, mode);
    preview_.setPausedJudgeAreaView(pausedJudgeArea);
}
void MobileExportComposition::applyRuntimeAudioSettings(const PreviewAudioSettings& value) {
    audioSettings_ = value; audioSettings_.normalize(); preview_.applyAudioSettings(audioSettings_);
    audioAudition_.applyLevels(audioSettings_);
}
void MobileExportComposition::applyAudioSettings(const PreviewAudioSettings& value) {
    breakSlideTailCheerMutedPreference_ = value.breakSlideTailCheerMuted;
    applyRuntimeAudioSettings(value);
    QSettings preferences;
    preferences.setValue("mobile/breakSlideTailCheerMuted", breakSlideTailCheerMutedPreference_);
    saveProjectAudioPreferences();
}
void MobileExportComposition::saveAudioSettingsAsSoftwareDefault() {
    QSettings settings; settings.setValue("mobile/audio", QJsonDocument(audioSettings_.toJson()).toJson(QJsonDocument::Compact));
}
void MobileExportComposition::restoreAudioSettingsFromSoftwareDefault() {
    QSettings settings; const auto json = QJsonDocument::fromJson(settings.value("mobile/audio").toByteArray());
    applyAudioSettings(previewAudioSettingsWithBreakSlideTailCheerPreference(
        json.isObject() ? PreviewAudioSettings::fromJson(json.object()) : PreviewAudioSettings{},
        breakSlideTailCheerMutedPreference_));
}

void MobileExportComposition::loadProjectAudioPreferences() {
    const QString path = document_.currentFilePath();
    if (!audioProjectPath_.isEmpty() && audioProjectPath_ == path) return;
    audioSettingsModel_.releaseAudition();
    QSettings settings;
    const auto defaults = QJsonDocument::fromJson(settings.value("mobile/audio").toByteArray());
    const PreviewAudioSettings softwareDefault = defaults.isObject()
        ? PreviewAudioSettings::fromJson(defaults.object()) : PreviewAudioSettings{};
    breakSlideTailCheerMutedPreference_ = settings.value("mobile/breakSlideTailCheerMuted",
        softwareDefault.breakSlideTailCheerMuted).toBool();
    const auto stored = project_preferences::load(path).value("preview_audio");
    const bool adopt = !stored.isObject() && editedAudioWithoutProject_;
    const PreviewAudioSettings loaded = stored.isObject() ? PreviewAudioSettings::fromJson(stored.toObject())
        : adopt ? audioSettings_ : softwareDefault;
    audioProjectPath_ = path;
    editedAudioWithoutProject_ = false;
    applyRuntimeAudioSettings(previewAudioSettingsWithBreakSlideTailCheerPreference(loaded,
        breakSlideTailCheerMutedPreference_));
    if (adopt && !path.isEmpty()) saveProjectAudioPreferences();
    emit audioSettingsModel_.changed();
}

void MobileExportComposition::saveProjectAudioPreferences() {
    const QString path = document_.currentFilePath();
    if (path.isEmpty()) { editedAudioWithoutProject_ = true; return; }
    auto preferences = project_preferences::load(path);
    preferences.insert("preview_audio", audioSettings_.toJson());
    if (!project_preferences::save(path, preferences))
        requests_.postNotice(NoticeSeverity::Error, qtTrId("action.audio_settings"),
            tr("无法保存工程音量配置；当前音量仍在本次会话中生效。"));
}

void MobileExportComposition::savePreviewPreferences() {
    if (restoringPreferences_) return;
    QSettings settings;
    auto stored = QJsonDocument::fromJson(settings.value("mobile/preview").toByteArray()).object();
    auto render = stored.value("render").toObject();
    auto values = renderSettings();
    for (const auto* key : {"layoutSquareScaleMin", "layoutSquareScaleMax", "layoutSquareScaleStep",
                           "flowSpeedMin", "flowSpeedMax", "flowSpeedStep"}) values.remove(key);
    const auto& live = preview_.sceneRuntime().frameState().render;
    values.insert("showObjectStatsHud", live.showObjectStatsHud);
    values.insert("showChartInfoHud", live.showChartInfoHud);
    values.insert("fixHudTextLayout", live.fixHudTextLayout);
    for (auto it = values.cbegin(); it != values.cend(); ++it) render.insert(it.key(), QJsonValue::fromVariant(it.value()));
    stored.insert("schema", 1); stored.insert("render", render);
    stored.insert("skin", appearance_.skinDirectoryName());
    stored.insert("judgeEffectStyle", static_cast<int>(appearance_.judgeEffectStyle()));
    stored.insert("outline", static_cast<int>(appearance_.outlineVariant()));
    stored.insert("customOutline", customOutline_);
    settings.setValue("mobile/preview", QJsonDocument(stored).toJson(QJsonDocument::Compact));
    settings.sync();
    if (settings.status() != QSettings::NoError)
        requests_.postNotice(NoticeSeverity::Error, qtTrId("action.video_settings"),
            tr("无法保存预览设置；当前设置仍在本次会话中生效。"));
}

void MobileExportComposition::restorePreviewPreferences() {
    QScopedValueRollback guard(restoringPreferences_, true);
    QSettings settings;
    const auto stored = QJsonDocument::fromJson(settings.value("mobile/preview").toByteArray()).object();
    const auto render = stored.value("render").toObject();
    for (auto it = render.begin(); it != render.end(); ++it) setRenderSetting(it.key(), it.value().toVariant());
    const auto requestedSkin = stored.value("skin").toString();
    for (const auto& name : availableSkinDirectoryNames()) {
        if (name.compare(requestedSkin, Qt::CaseInsensitive) == 0) { appearance_.setSkinDirectory(name); break; }
    }
    if (stored.value("judgeEffectStyle").isDouble())
        appearance_.setJudgeEffectStyle(static_cast<PreviewJudgeEffectStyle>(qBound(0, stored.value("judgeEffectStyle").toInt(), 1)));
    if (stored.value("outline").isDouble())
        appearance_.setOutlineVariant(static_cast<PreviewOutlineVariant>(qBound(0, stored.value("outline").toInt(), 3)));
    const QString custom = stored.value("customOutline").toString();
    if (availableCustomOutlineFileNames().contains(custom)) customOutline_ = custom;
    refreshSurfaces();
}
}
