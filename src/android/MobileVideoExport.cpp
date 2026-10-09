#include "MobileVideoExport.h"
#include "ExportDestination.h"
#include "common/AssetPaths.h"
#include "common/IntroConfig.h"
#include "common/ContentDurationConfig.h"
#include "common/ChartAssetPaths.h"
#include "timeline/TimelineMarkerOffset.h"
#include "tools/video_export/VideoExportPauseOverlay.h"
#include "tools/video_export/FontLibrary.h"
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QImageReader>
#include <QUuid>
#include <QDir>
#include <QDebug>
#include <QGuiApplication>
#include <stdexcept>
#ifdef Q_OS_ANDROID
#include <QJniEnvironment>
#endif

namespace miacode::android {
MobileVideoExport::MobileVideoExport(AndroidDocumentSession& document, MobilePreview& preview, QObject* parent)
    : QObject(parent), document_(document), preview_(preview) {
    timer_.setInterval(5);
    connect(&timer_, &QTimer::timeout, this, &MobileVideoExport::advance);
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (running_ && state != Qt::ApplicationActive && !document_.backgroundExportAllowed()) cancel();
    });
}
MobileVideoExport::~MobileVideoExport() {
    cancel();
    if (audioWorker_.joinable()) audioWorker_.join();
    endBackgroundService();
}

bool MobileVideoExport::beginBackgroundService(QString* error) {
#ifdef Q_OS_ANDROID
    if (!backgroundServiceActive_ && document_.backgroundExportAllowed()) {
        QJniObject::callStaticMethod<void>("org/miacode/android/ChartExportService", "begin", "(Landroid/content/Context;)V",
            QNativeInterface::QAndroidApplication::context().object());
        QJniEnvironment env;
        if (env.checkAndClearExceptions()) {
            if (error) *error = QStringLiteral("Background export service could not start");
            return false;
        }
        backgroundServiceActive_ = true;
    }
#else
    Q_UNUSED(error);
#endif
    return true;
}
void MobileVideoExport::endBackgroundService() {
#ifdef Q_OS_ANDROID
    if (backgroundServiceActive_) {
        QJniObject::callStaticMethod<void>("org/miacode/android/ChartExportService", "end", "(Landroid/content/Context;)V",
            QNativeInterface::QAndroidApplication::context().object());
        QJniEnvironment env;
        env.checkAndClearExceptions();
    }
#endif
    backgroundServiceActive_ = false;
}
bool MobileVideoExport::beginBatchExecution(QString* error) {
    if (batchActive_ || running_) {
        if (error) *error = QStringLiteral("An export is already running");
        return false;
    }
    if (!beginBackgroundService(error)) return false;
    batchActive_ = true;
    batchCanceled_ = false;
    return true;
}
void MobileVideoExport::endBatchExecution() {
    batchActive_ = false;
    endBackgroundService();
}
void MobileVideoExport::updateBatchProgress(int percent) {
#ifdef Q_OS_ANDROID
    if (batchActive_ && backgroundServiceActive_)
        QJniObject::callStaticMethod<void>("org/miacode/android/ChartExportService", "progress", "(I)V", qBound(0, percent, 100));
#else
    Q_UNUSED(percent);
#endif
}

VideoExportTask MobileVideoExport::buildTask(int id) const {
    if (id == 0) id = document_.activeDifficulty();
    const auto& doc = document_.workspace().document();
    const auto* difficulty = doc.difficulty(id);
    VideoExportTask task;
    if (!difficulty) return task;
    const auto parsed = SimaiNativeParser::parseForTimeline(difficulty->chart, simai::buildTimingMetadata(doc));
    const double first = timeline::offset::parsedFirstSeconds(doc.first);
    task.noteMarkers = timeline::offset::shiftedNoteMarkers(parsed.noteMarkers, first,
        timeline::offset::NonFiniteHandling::PassThrough);
    task.chartPath = document_.currentFilePath();
    task.trackPath = document_.previewAssetPath("audio");
    task.backgroundMediaPath = document_.previewAssetPath("video");
    if (task.backgroundMediaPath.isEmpty()) task.backgroundMediaPath = document_.previewAssetPath("image");
    task.skinDirectory = preview_.sceneRuntime().skinDirectory();
    task.muriRenderOptions = preview_.muriRenderOptions();
    task.staticTapOnSlideThresholdSeconds = preview_.muriTapOnSlideThresholdMs() / 1000.0;
    double chartEnd = 0;
    for (const auto& marker : task.noteMarkers) {
        chartEnd = qMax(chartEnd, qMax(qMax(marker.second, marker.endSecond),
            qMax(marker.slideTraceSecond, marker.availableSecond)));
        for (double second : marker.slideSegmentShootSeconds) chartEnd = qMax(chartEnd, second);
    }
    task.contentDurationSeconds = content_duration::totalContentDurationSeconds(chartEnd, preview_.trackDurationSeconds());
    task.chartTitle = doc.title; task.chartArtist = doc.artist;
    task.chartDesigner = difficulty->designer.trimmed().isEmpty() ? doc.designer : difficulty->designer;
    task.chartDifficultyLabel = SimaiDocument::difficultyShortName(id) + " " + difficulty->level;
    task.intro.title = doc.title; task.intro.artist = doc.artist; task.intro.designer = task.chartDesigner;
    task.intro.level = difficulty->level;
    task.intro.difficulty = SimaiDocument::difficultyName(id).toUpper();
    task.intro.jacketPath = document_.previewAssetPath("image");
    task.intro.mode = detectedIntroBannerMode(task.noteMarkers);
    for (const auto& field : doc.extraFields) if (field.key == "clock_count") task.clockCount = field.value.toInt();
    const QRegularExpression bpmExpression(QStringLiteral("\\(([0-9]+(?:\\.[0-9]+)?)\\)"));
    const auto bpm = bpmExpression.match(difficulty->chart);
    if (bpm.hasMatch()) { task.clockBpm = bpm.captured(1).toDouble(); task.intro.bpm = bpm.captured(1); }
    QString name = doc.title + "-" + SimaiDocument::difficultyName(id);
    name.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
    task.outputPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/exports/" + name + ".mp4";
    return task;
}

bool MobileVideoExport::start(const VideoExportTask& task, QString* error, bool independentChart) {
    const auto reject = [error](const QString& message) { if (error) *error = message; return false; };
    if (running_) return reject(QStringLiteral("An export is already running"));
    if (batchActive_ && (batchCanceled_ || !independentChart))
        return reject(QStringLiteral("Batch export is canceled or already owns the exporter"));
#ifdef Q_OS_ANDROID
    if (!document_.backgroundExportAllowed() && qGuiApp->applicationState() != Qt::ApplicationActive)
        return reject(QStringLiteral("Return to the app to start export, or enable background export"));
#endif
    if (task.outputPath.isEmpty() || !qIsFinite(task.contentDurationSeconds) || task.contentDurationSeconds <= 0
        || !qIsFinite(task.exportStartSeconds) || task.exportStartSeconds < 0 || task.fps <= 0
        || task.outputWidth <= 0 || task.outputHeight <= 0 || task.outputWidth % 2 || task.outputHeight % 2
        || qint64(task.outputWidth) * task.outputHeight > 0x7fffffffLL / 4)
        return reject(QStringLiteral("Invalid export output, range or frame format"));
    if (!independentChart) {
        bool validFirst = false;
        timeline::offset::parsedFirstSeconds(document_.metadataFirst(), &validFirst);
        if (!validFirst) return reject(QStringLiteral("The chart offset must be a finite number"));
    }
    if (!video_export::buildVideoExportAudioRenderPlan(task, &plan_, error)) return false;
    if (!batchActive_ && !beginBackgroundService(error)) return false;
    if (audioWorker_.joinable()) audioWorker_.join();
    task_ = task; cancelled_.store(false); running_ = true; error_.clear();
    const auto sizePolicy = video_export::videoExportSizePolicy(task_.sizePreset);
    if (sizePolicy.disableVideoBackground && chart_assets::isVideoBackgroundPath(task_.backgroundMediaPath)) {
        task_.backgroundMediaPath = chart_assets::resolvePreferredBackgroundMediaPath(
            task_.chartPath, task_.backgroundMediaPath, false);
    }
    percent_ = 0; frame_ = 0; videoRequested_ = false; inputFinished_ = false; pendingFrame_ = {};
    decodedVideoUs_ = -1;
    publicationToken_.clear(); publishedUri_.clear(); publicationDetails_ = {};
    directory_ = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/export-jobs/" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    state_ = {};
    state_.noteMarkers = task.noteMarkers;
    auto statistics = std::make_shared<preview::scene::PreviewProgressStatsCache>();
    statistics->rebuild(task.noteMarkers); state_.progressStatsCache = statistics;
    state_.muriRenderOptions = task.muriRenderOptions; state_.muriAnalysisReport = task.muriAnalysisReport;
    state_.chartTitle = task.chartTitle; state_.chartArtist = task.chartArtist;
    state_.chartDesigner = task.chartDesigner; state_.chartDifficultyLabel = task.chartDifficultyLabel;
    state_.render.backgroundBrightnessOuter = task.backgroundBrightnessOuter;
    state_.render.backgroundBrightnessInner = task.backgroundBrightnessInner;
    state_.render.layoutSquareScale = task.layoutSquareScale; state_.render.smoothBrightness = task.smoothBrightness;
    state_.render.backgroundScaleMode = task.backgroundScaleMode;
    state_.render.tapFlowSpeed = task.tapFlowSpeed; state_.render.touchFlowSpeed = task.touchFlowSpeed;
    state_.render.slideEarlierSecondAndTextOnTop = task.slideEarlierSecondAndTextOnTop;
    state_.render.tapJudgeTextDistance = task.tapJudgeTextDistance; state_.render.judgeEffectStyle = task.judgeEffectStyle;
    state_.render.showChartInfoHud = task.showChartInfoHud; state_.render.fixHudTextLayout = task.fixHudTextLayout;
    state_.render.centerDisplayMode = task.centerDisplayMode;
    state_.media = {};
    const auto audioPlan = plan_;
    const auto directory = directory_;
    emit changed();
    audioWorker_ = std::thread([this, audioPlan, directory, capturedTask = task_] {
        QString wav, error;
        preview::runtime::PreviewSceneAssetLoadResult assets;
        try {
            if (!capturedTask.outputPath.endsWith(".wav", Qt::CaseInsensitive) && !cancelled_.load()) {
                assets = preview::runtime::PreviewSceneAssetLoader::load(capturedTask.skinDirectory,
                    capturedTask.outlineVariant, 0, capturedTask.outlineImagePath);
                if (assets.skinAssets.tapImage.isNull() || assets.skinAssets.holdImage.isNull() || assets.skinAssets.starImage.isNull())
                    throw std::runtime_error("Export skin is missing required note images");
            }
            wav = renderExportWav(audioPlan, capturedTask.introSoundFileName, capturedTask.introSoundVolume, directory, cancelled_);
        }
        catch (const std::exception& failure) { error = QString::fromUtf8(failure.what()); }
        QMetaObject::invokeMethod(this, [this, wav, error, assets = std::move(assets)]() mutable {
            prepared(wav, error, std::move(assets));
        }, Qt::QueuedConnection);
    });
    return true;
}

bool MobileVideoExport::commitFile(const QString& source, const QString& destination, QString* error) {
    QFile input(source);
    QSaveFile output(destination);
    if (!QDir().mkpath(QFileInfo(destination).absolutePath()) || !input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly)) {
        if (error) *error = QStringLiteral("Cannot open export output: %1").arg(output.errorString()); return false;
    }
    while (!input.atEnd()) {
        if (cancelled_.load()) { if (error) *error = QStringLiteral("Export cancelled"); return false; }
        const auto bytes = input.read(1024 * 1024);
        if (bytes.isEmpty() || output.write(bytes) != bytes.size()) {
            if (error) *error = QStringLiteral("Cannot write export output"); return false;
        }
    }
    if (cancelled_.load()) { if (error) *error = QStringLiteral("Export cancelled"); return false; }
    if (!output.commit()) { if (error) *error = output.errorString(); return false; }
    return true;
}

void MobileVideoExport::prepared(const QString& wav, const QString& error, preview::runtime::PreviewSceneAssetLoadResult assets) {
    if (audioWorker_.joinable()) audioWorker_.join();
    if (cancelled_.load() || !error.isEmpty()) { complete(false, error.isEmpty() ? QStringLiteral("Export cancelled") : error); return; }
    wav_ = wav;
    if (task_.outputPath.endsWith(".wav", Qt::CaseInsensitive)) {
        finishOutput(wav, task_.outputPath); return;
    }
    // Resolve the captured skin/outline independently of asynchronous live-preview
    // loads and the temporary paused judge-area view.
    state_.assets = std::move(assets.assetState);
    state_.skin = std::move(assets.skinAssets);
    state_.judgeOverlay = std::move(assets.judgeOverlayAssets);
    state_.judgeEffect = std::move(assets.judgeEffectAssets);
    state_.touchPadAuthoringEnabled = false;
    state_.hoveredTouchPad.clear(); state_.pressedTouchPad.clear();
    QString failure;
    renderer_ = std::make_unique<PreviewQuickExportSession>();
    renderer_->setFrameSize(QSize(task_.outputWidth, task_.outputHeight));
    renderer_->setFrameState(state_);
    if (!renderer_->initialize(QSurfaceFormat::defaultFormat(), nullptr, &failure)) { complete(false, failure); return; }
    if (task_.intro.enabled && task_.fullRangeExport) {
        if (!renderer_->setupIntroOverlay(QUrl(QString::fromLatin1(intro::kOverlayQmlUrl)), &failure)) { complete(false, failure); return; }
        QFile file(QStringLiteral(":/intro/templates/maimai_banner.json"));
        if (!file.open(QIODevice::ReadOnly)) { complete(false, QStringLiteral("Cannot read intro template")); return; }
        QVariantMap banner = QJsonDocument::fromJson(file.readAll()).object().toVariantMap();
        video_export::applyBannerFontOverride(banner, task_.intro.fontDisplayPath, task_.intro.fontBodyPath);
        renderer_->setIntroBannerData(introBannerTrackMap(task_.intro), banner,
            chart_assets::displayBackgroundImageUrl(task_.intro.jacketPath),
            QUrl(QString::fromLatin1(intro::kLogoFallbackUrl)), introBannerStyleMap(task_.intro));
    }
    if (!chart_assets::isVideoBackgroundPath(task_.backgroundMediaPath)) {
        QImageReader image(task_.backgroundMediaPath);
        image.setDecideFormatFromContent(true);
        if (!task_.backgroundMediaPath.isEmpty() && image.canRead()) {
            state_.media.mediaFrame = image.read(); state_.media.stageMediaAvailable = !state_.media.mediaFrame.isNull();
            if (!state_.media.stageMediaAvailable) { complete(false, image.errorString()); return; }
            ++state_.media.stageMediaSerial;
        }
    }
#ifdef Q_OS_ANDROID
    const QString video = chart_assets::isVideoBackgroundPath(task_.backgroundMediaPath) ? task_.backgroundMediaPath : QString();
    const int audioBitrate = video_export::effectiveVideoExportAudioBitrateKbps(task_.sizePreset, task_.audioBitrateKbps);
    const int videoBitrate = static_cast<int>(video_export::videoExportTargetBitrateKbps(
        task_.preset == VideoExportPreset::HighQuality, task_.sizePreset,
        task_.outputWidth, task_.outputHeight, task_.fps) * 1000);
    encoder_ = QJniObject("org/miacode/android/ChartExportEncoder", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;IIIIIII)V",
        QJniObject::fromString(directory_ + "/video.mp4").object<jstring>(), QJniObject::fromString(wav_).object<jstring>(),
        QJniObject::fromString(video).object<jstring>(), task_.outputWidth, task_.outputHeight, task_.fps,
        plan_.frameCount, audioBitrate, videoBitrate, video_export::videoExportSizePolicy(task_.sizePreset).gopSeconds);
    if (!encoder_.isValid()) { complete(false, QStringLiteral("Cannot create Android chart encoder")); return; }
#else
    // Host validation emits three rendered samples and the exact WAV. Actual
    // MP4 encoding is verified separately through the Android codec worker.
    if (chart_assets::isVideoBackgroundPath(task_.backgroundMediaPath)) {
        QImageReader image(task_.intro.jacketPath);
        image.setDecideFormatFromContent(true);
        state_.media.mediaFrame = image.read();
        if (!task_.intro.jacketPath.isEmpty() && state_.media.mediaFrame.isNull()) {
            complete(false, image.errorString()); return;
        }
        state_.media.stageMediaAvailable = !state_.media.mediaFrame.isNull(); ++state_.media.stageMediaSerial;
    }
#endif
    percent_ = 10; emit changed(); timer_.start();
}

void MobileVideoExport::advance() {
#ifdef Q_OS_ANDROID
    const auto status = QJsonDocument::fromJson(encoder_.callObjectMethod("status", "()Ljava/lang/String;").toString().toUtf8()).object();
    if (!status.value("error").toString().isEmpty() || cancelled_.load()) {
        encoder_.callMethod<void>("cancel");
        if (status.value("done").toBool()) complete(false, status.value("error").toString().isEmpty()
            ? (error_.isEmpty() ? QStringLiteral("Export cancelled") : error_) : status.value("error").toString());
        return;
    }
    if (inputFinished_) {
        if (!status.value("done").toBool()) return;
        finishOutput(directory_ + "/video.mp4", task_.outputPath); return;
    }
    if (!encoder_.callMethod<jboolean>("hasCapacity")) return;
#else
    if (cancelled_.load()) { complete(false, QStringLiteral("Export cancelled")); return; }
#endif
    if (frame_ >= plan_.frameCount) {
#ifdef Q_OS_ANDROID
        encoder_.callMethod<void>("finishInput"); inputFinished_ = true;
#else
        finishOutput(wav_, task_.outputPath + ".wav");
#endif
        return;
    }
    const double outputSecond = frame_ / static_cast<double>(task_.fps);
    const double chartSecond = task_.fullRangeExport ? plan_.timelineOriginSecond + outputSecond
        : plan_.segmentStartSecond + qMax(0.0, outputSecond - plan_.leadInSeconds);
    if (pendingFrame_.isNull()) {
#ifdef Q_OS_ANDROID
        const qint64 videoUs = qRound64(qMax(0.0, chartSecond) * 1000000);
        if (chart_assets::isVideoBackgroundPath(task_.backgroundMediaPath) && decodedVideoUs_ != videoUs) {
            if (!videoRequested_) { encoder_.callMethod<void>("requestVideoFrame", "(J)V", videoUs); videoRequested_ = true; return; }
            const auto pixels = encoder_.callObjectMethod<jbyteArray>("takeVideoFrame");
            if (!pixels.isValid()) return;
            const int width = encoder_.callMethod<jint>("videoWidth"), height = encoder_.callMethod<jint>("videoHeight");
            QJniEnvironment env;
            if (width <= 0 || height <= 0 || env->GetArrayLength(pixels.object<jbyteArray>()) != qint64(width) * height * 4) {
                cancel(); error_ = QStringLiteral("PV decoder returned invalid frame dimensions"); return;
            }
            QImage image(width, height, QImage::Format_RGBA8888);
            if (image.isNull()) { cancel(); error_ = QStringLiteral("Cannot allocate PV export frame"); return; }
            env->GetByteArrayRegion(pixels.object<jbyteArray>(), 0, width * height * 4, reinterpret_cast<jbyte*>(image.bits()));
            if (env.checkAndClearExceptions()) { cancel(); error_ = QStringLiteral("Cannot copy decoded PV frame"); return; }
            // Keep one retained texture for decoded video, rather than putting
            // every distinct PV frame into the static sprite texture cache.
            state_.media.resolvedStageImage = image;
            state_.media.resolvedStageImageCacheable = false;
            state_.media.stageMediaAvailable = true; ++state_.media.stageMediaSerial;
            decodedVideoUs_ = videoUs; videoRequested_ = false;
        }
#endif
        QString failure;
        renderer_->setFrameState(state_);
        const int introFrame = intro::authoringFrameForOutputFrame(frame_, task_.fps);
        const bool covered = task_.intro.enabled && task_.fullRangeExport && introFrame < intro::kHudRevealFrame;
        renderer_->setIntroFrame(introFrame, task_.intro.enabled && task_.fullRangeExport && frame_ < plan_.introFrameCount);
        renderer_->applyExportFrameTick(chartSecond, task_.showTimestamp && !covered, task_.showObjectStatsHud && !covered, true, 0, task_.fps);
        pendingFrame_ = renderer_->renderFrame(&failure).convertToFormat(QImage::Format_RGBA8888);
        if (pendingFrame_.isNull()) { complete(false, failure.isEmpty() ? QStringLiteral("Chart render returned an empty frame") : failure); return; }
        if (!task_.fullRangeExport && outputSecond < plan_.leadInSeconds)
            video_export::detail::drawLeadInPauseOverlay(&pendingFrame_);
    }
#ifdef Q_OS_ANDROID
    QJniEnvironment env;
    jbyteArray bytes = env->NewByteArray(pendingFrame_.sizeInBytes());
    if (!bytes) { env.checkAndClearExceptions(); cancel(); error_ = QStringLiteral("Cannot allocate encoder input"); return; }
    env->SetByteArrayRegion(bytes, 0, pendingFrame_.sizeInBytes(), reinterpret_cast<const jbyte*>(pendingFrame_.constBits()));
    if (env.checkAndClearExceptions()) {
        env->DeleteLocalRef(bytes); cancel(); error_ = QStringLiteral("Cannot copy encoder input"); return;
    }
    const bool accepted = encoder_.callMethod<jboolean>("enqueue", "([B)Z", bytes);
    env->DeleteLocalRef(bytes);
    if (!accepted) return;
#else
    if (frame_ == 0 || frame_ == plan_.frameCount / 2 || frame_ == plan_.frameCount - 1
        || (task_.intro.enabled && frame_ == qRound(task_.fps * 3.2))) {
        const QString imagePath = task_.outputPath + QStringLiteral("-%1.png").arg(frame_, 6, 10, QLatin1Char('0'));
        if (!pendingFrame_.save(imagePath)) { complete(false, QStringLiteral("Cannot write host render sample")); return; }
    }
#endif
    pendingFrame_ = {}; ++frame_;
    const int next = 10 + frame_ * 85 / plan_.frameCount;
    if (next != percent_) {
        percent_ = next; emit changed();
#ifdef Q_OS_ANDROID
        if (!batchActive_ && backgroundServiceActive_)
            QJniObject::callStaticMethod<void>("org/miacode/android/ChartExportService", "progress", "(I)V", percent_);
#endif
    }
}

void MobileVideoExport::cancel() {
    if (batchActive_) batchCanceled_ = true;
    cancelled_.store(true);
#ifdef Q_OS_ANDROID
    if (encoder_.isValid()) encoder_.callMethod<void>("cancel");
    if (!publicationToken_.isEmpty()) QJniObject::callStaticMethod<void>("org/miacode/android/ExportFilePublisher", "cancel",
        "(Ljava/lang/String;)V", QJniObject::fromString(publicationToken_).object<jstring>());
#endif
}
void MobileVideoExport::finishOutput(const QString& source, const QString& destination) {
    timer_.stop();
    if (audioWorker_.joinable()) audioWorker_.join();
    audioWorker_ = std::thread([this, source, destination] {
        QString failure;
        const bool ok = commitFile(source, destination, &failure);
        QMetaObject::invokeMethod(this, [this, destination, ok, failure] {
            if (audioWorker_.joinable()) audioWorker_.join();
            if (!ok || cancelled_.load()) { complete(false, failure.isEmpty() ? QStringLiteral("Export cancelled") : failure); return; }
#ifdef Q_OS_ANDROID
            const auto target = exportDestination(destination);
            if (!target.isEmpty()) {
                publicationToken_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
                QJniObject::callStaticMethod<void>("org/miacode/android/ExportFilePublisher", "publish",
                    "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
                    QNativeInterface::QAndroidApplication::context().object(), QJniObject::fromString(publicationToken_).object<jstring>(),
                    QJniObject::fromString(destination).object<jstring>(),
                    QJniObject::fromString(QString::fromUtf8(QJsonDocument(target).toJson(QJsonDocument::Compact))).object<jstring>());
                QJniEnvironment env;
                if (env.checkAndClearExceptions()) { complete(false, QStringLiteral("Cannot write the selected destination; private export retained")); return; }
                percent_ = 95; emit changed(); return;
            }
#endif
            complete(true);
        }, Qt::QueuedConnection);
    });
}
void MobileVideoExport::publicationUpdate(const QJsonObject& result) {
    if (!running_ || publicationToken_.isEmpty() || result.value("token").toString() != publicationToken_) return;
    if (!result.value("done").toBool()) {
        percent_ = qBound(95, 95 + result.value("percent").toInt() / 20, 99);
#ifdef Q_OS_ANDROID
        if (!batchActive_ && backgroundServiceActive_) QJniObject::callStaticMethod<void>("org/miacode/android/ChartExportService", "progress", "(I)V", percent_);
#endif
        emit changed(); return;
    }
    publicationDetails_ = result;
    publishedUri_ = result.value("uri").toString();
    complete(result.value("ok").toBool() && !cancelled_.load(), cancelled_.load()
        ? QStringLiteral("Export cancelled; private export retained") : result.value("error").toString());
}
void MobileVideoExport::complete(bool success, const QString& error) {
    timer_.stop(); renderer_.reset(); pendingFrame_ = {};
#ifdef Q_OS_ANDROID
    if (encoder_.isValid() && !success) encoder_.callMethod<void>("cancel");
    encoder_ = {};
#endif
    if (!batchActive_) endBackgroundService();
    running_ = false; percent_ = success ? 100 : percent_; error_ = error;
    QSaveFile report(directory_ + "/report.json");
    if (report.open(QIODevice::WriteOnly)) {
        const QImage& stageImage = state_.media.resolvedStageImage.isNull() ? state_.media.mediaFrame : state_.media.resolvedStageImage;
        const QJsonObject details{{"schema", 1}, {"success", success}, {"error", error}, {"output", task_.outputPath},
            {"publishedUri", publishedUri_}, {"publication", publicationDetails_},
            {"wav", wav_}, {"renderedFrames", frame_}, {"expectedFrames", plan_.frameCount}, {"fps", task_.fps},
            {"width", task_.outputWidth}, {"height", task_.outputHeight}, {"startSecond", plan_.segmentStartSecond},
            {"qualityPreset", task_.preset == VideoExportPreset::HighQuality ? "high_quality" : "fast"},
            {"sizePreset", video_export::videoExportSizePresetToken(task_.sizePreset)},
            {"targetVideoBitrateKbps", video_export::videoExportTargetBitrateKbps(
                task_.preset == VideoExportPreset::HighQuality, task_.sizePreset, task_.outputWidth, task_.outputHeight, task_.fps)},
            {"requestedAudioBitrateKbps", task_.audioBitrateKbps},
            {"effectiveAudioBitrateKbps", video_export::effectiveVideoExportAudioBitrateKbps(task_.sizePreset, task_.audioBitrateKbps)},
            {"backgroundMediaPath", task_.backgroundMediaPath},
            {"endSecond", plan_.segmentEndSecond}, {"leadInSeconds", plan_.leadInSeconds},
            {"introSeconds", plan_.introLeadSeconds}, {"durationSeconds", plan_.alignedTotalSeconds},
            {"stageImageWidth", stageImage.width()}, {"stageImageHeight", stageImage.height()},
            {"sfxPlaybacks", plan_.scheduledSfxPlaybacks.size()}, {"touchholdSpans", plan_.touchholdSpanPlaybacks.size()}};
        report.write(QJsonDocument(details).toJson()); report.commit();
    }
    qInfo() << "Mobile chart export:" << (success ? "passed" : "failed") << task_.outputPath << error;
    const QString publishedPath = publicationDetails_.value("displayPath").toString();
    emit changed(); emit finished(success, publishedPath.isEmpty() ? task_.outputPath : publishedPath, error);
}
}
