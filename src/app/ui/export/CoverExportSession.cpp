#include "common/LocalizedText.h"

#include "app/ui/export/CoverExportSession.h"
#include "app/ui/document/DifficultyOptions.h"
#include "app/ui/preferences/LocaleService.h"

#include "core/chart/ChartAssetPaths.h"
#include "core/chart/document/SimaiDocument.h"
#include "app/services/PlaybackControl.h"
#include "export/cover_export/CoverCompositionState.h"
#include "app/services/CoverExportPreferences.h"
#include "app/services/ProjectPreferences.h"
#include "export/cover_export/CoverFramePlaybackController.h"
#include "export/cover_export/CoverFrameExportPlan.h"
#include "export/cover_export/CoverFrameSceneBinder.h"
#include "export/cover_export/CoverLayoutModel.h"
#include "export/cover_export/SceneFrameRenderer.h"
#include "export/video_export/FontLibrary.h"
#include "app/services/UserFontLibrary.h"
#include "core/scene/PreviewLayerOrder.h"
#include "preview/quick_scene/PreviewQuickSceneRoot.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QJsonDocument>
#include <QUrl>

#include <iterator>
#include <algorithm>


namespace miacode::ui {
namespace {

struct CoverResolutionPreset {
    int width;
    int height;
    const char* label;
};

constexpr CoverResolutionPreset kCoverResolutionPresets[] = {
    {720, 720, "720×720 (1:1)"},
    {960, 720, "960×720 (4:3)"}, {1280, 720, "1280×720 (16:9)"},
    {1080, 1080, "1080×1080 (1:1)"}, {1440, 1080, "1440×1080 (4:3)"},
    {1920, 1080, "1920×1080 (16:9)"}, {1440, 1440, "1440×1440 (1:1)"},
    {1920, 1440, "1920×1440 (4:3)"}, {2560, 1440, "2560×1440 (16:9)"},
};

QVariantMap loadBannerTemplate()
{
    QFile file(QStringLiteral(":/intro/templates/maimai_banner.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    return document.isObject() ? document.object().toVariantMap() : QVariantMap{};
}

// Where a relative output file is read against: the chart folder.
QString coverOutputBaseDirectory(const QString& chartPath)
{
    const QFileInfo chartInfo(chartPath);
    const QDir directory = chartInfo.absoluteDir();
    return directory.exists() ? directory.absolutePath() : QDir::currentPath();
}

}  // namespace

CoverExportSession::CoverExportSession(miacode::ExportEngine& exportEngine,
                                             miacode::UiRequestService& uiRequests,
                                             miacode::PlaybackControl*& playbackControlSlot,
                                             QObject* parent)
    : QObject(parent)
    , exportEngine_(exportEngine)
    , uiRequests_(&uiRequests)
    , playbackControlSlot_(&playbackControlSlot)
    , layout_(std::make_unique<miacode::cover_export::CoverLayoutModel>())
    , playback_(std::make_unique<miacode::cover_export::CoverFramePlaybackController>(this))
    , sceneBinder_(std::make_unique<miacode::cover_export::CoverFrameSceneBinder>(this))
    , bannerTemplate_(loadBannerTemplate())
{

    compositionSaveTimer_.setSingleShot(true);
    compositionSaveTimer_.setInterval(500);
    connect(&compositionSaveTimer_, &QTimer::timeout, this, &CoverExportSession::flushComposition);
    connect(&miacode::LocaleService::instance(), &miacode::LocaleService::languageChanged,
            this, [this](const QString&) {
        emit localeLabelsChanged();
        emit fontLibraryChanged();
    });
    layout_->ensureDefaultLayers();
    activeLayerKey_ = miacode::cover_export::CoverLayoutModel::cardKey();
    connect(playback_.get(), &miacode::cover_export::CoverFramePlaybackController::secondsChanged,
            this, &CoverExportSession::onPlaybackSecondsChanged);
    connect(playback_.get(), &miacode::cover_export::CoverFramePlaybackController::reachedEnd,
            this, &CoverExportSession::onPlaybackReachedEnd);
    connect(playback_.get(), &miacode::cover_export::CoverFramePlaybackController::playingChanged,
            this, &CoverExportSession::chartFramePlayingChanged);
    connect(sceneBinder_.get(), &miacode::cover_export::CoverFrameSceneBinder::liveChartSceneBoundChanged,
            this, &CoverExportSession::liveChartSceneBoundChanged);
}

CoverExportSession::~CoverExportSession()
{
    flushComposition();
    stopAndDetachLiveChartScene();
    if (auto* scene = qobject_cast<PreviewQuickSceneRoot*>(lastLiveChartScene_.data())) {
        scene->setFrameState(nullptr);
    }
    lastLiveChartScene_.clear();
}

QObject* CoverExportSession::uiRequests() const { return uiRequests_; }
QObject* CoverExportSession::layoutModel() const { return layout_.get(); }
QObject* CoverExportSession::activeLayer() const { return activeCoverLayer(); }

QVariantMap CoverExportSession::templateMap() const
{
    QVariantMap result = bannerTemplate_;
    miacode::video_export::applyBannerFontOverride(result, cardFontDisplayPath_, cardFontBodyPath_);
    return result;
}

QVariantMap CoverExportSession::trackOverrides() const
{
    QVariantMap track;
    const IntroBannerSpec& banner = task_.intro;
    track.insert(QStringLiteral("title"), banner.title);
    track.insert(QStringLiteral("artist"), banner.artist);
    track.insert(QStringLiteral("designer"), banner.designer);
    track.insert(QStringLiteral("level"), banner.level);
    track.insert(QStringLiteral("difficulty"), banner.difficulty);
    track.insert(QStringLiteral("bpm"), banner.bpm);
    track.insert(QStringLiteral("mode"), isAutoIntroBannerMode(cardMode_)
        ? normalizedIntroBannerMode(banner.mode)
        : normalizedIntroBannerMode(cardMode_));
    track.insert(QStringLiteral("lvRenderMode"), levelTextRender_ ? QStringLiteral("text")
                                                                     : QStringLiteral("atlas"));
    track.insert(QStringLiteral("stillTextMode"), longTextMode_);
    return track;
}

QUrl CoverExportSession::jacketImage() const
{
    return miacode::chart_assets::displayBackgroundImageUrl(task_.intro.jacketPath);
}

QUrl CoverExportSession::backgroundImage() const
{
    return backgroundPath_.trimmed().isEmpty() ? QUrl() : QUrl::fromLocalFile(backgroundPath_);
}

int CoverExportSession::backgroundMode() const { return static_cast<int>(backgroundMode_); }

QVariantList CoverExportSession::fontLibraryOptions() const
{
    QVariantList output;
    const auto entries = miacode::video_export::fontLibraryEntries(
        miacode::app_preferences::fontLibraryDirectory(), true, qtTrId("card_font.default"));
    for (const auto& entry : entries) {
        output.append(QVariantMap{{QStringLiteral("label"), entry.label},
                                  {QStringLiteral("path"), entry.path},
                                  {QStringLiteral("family"), entry.family}});
    }
    return output;
}

QVariantList CoverExportSession::resolutionOptions() const
{
    QVariantList output;
    for (const auto& preset : kCoverResolutionPresets) {
        output.append(QVariantMap{{QStringLiteral("label"), QString::fromUtf8(preset.label)},
                                  {QStringLiteral("width"), preset.width},
                                  {QStringLiteral("height"), preset.height}});
    }
    return output;
}

int CoverExportSession::outputWidth() const { return kCoverResolutionPresets[resolutionIndex_].width; }
int CoverExportSession::outputHeight() const { return kCoverResolutionPresets[resolutionIndex_].height; }

QObject* CoverExportSession::chartSceneBinder() const
{
    return sceneBinder_.get();
}

bool CoverExportSession::chartFramePlaying() const
{
    return playback_ != nullptr && playback_->playing();
}

bool CoverExportSession::liveChartSceneBound() const
{
    return sceneBinder_ != nullptr && sceneBinder_->liveChartSceneBound();
}

double CoverExportSession::activeChartFrameSeconds() const
{
    auto* layer = activeCoverLayer();
    return layer != nullptr && layer->kind() == QStringLiteral("chartFrame")
        ? layer->frameSeconds() : 0.0;
}

QVariantList CoverExportSession::builtinPresets() const
{
    return {
        QVariantMap{{QStringLiteral("id"), QStringLiteral("card")},
                    {QStringLiteral("label"), qtTrId("cover.centered_card_default")},
                    {QStringLiteral("requiresChartFrame"), false}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("card_chart_frame")},
                    {QStringLiteral("label"), qtTrId("cover.card_chart_frame")},
                    {QStringLiteral("requiresChartFrame"), true}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("dual_chart_frames")},
                    {QStringLiteral("label"), qtTrId("cover.dual_chart_frame_collage")},
                    {QStringLiteral("requiresChartFrame"), true}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("pure_chart_frame")},
                    {QStringLiteral("label"), qtTrId("cover.pure_chart_frame")},
                    {QStringLiteral("requiresChartFrame"), true}},
    };
}

void CoverExportSession::enter(int preferredDifficultyId)
{
    if (!pageSessionActive_) {
        pageSessionActive_ = true;
        emit pageSessionActiveChanged();
    }
    rebuildDifficultyList();
    selectDifficulty(defaultDifficultyId(preferredDifficultyId));
    refreshSavedLists();
    emit fontLibraryChanged();
}

void CoverExportSession::refreshDocument(int preferredDifficultyId)
{
    rebuildDifficultyList();
    const int next = defaultDifficultyId(preferredDifficultyId);
    if (selectedDifficultyId_ != next) {
        selectedDifficultyId_ = next;
        emit selectedDifficultyIdChanged();
    }
    seedFromDifficulty(next);
}

void CoverExportSession::leave()
{
    if (!pageSessionActive_) {
        return;
    }
    commitActiveLayerFrameSeconds();
    stopAndDetachLiveChartScene();
    persistComposition();
    flushComposition();
    pageSessionActive_ = false;
    emit pageSessionActiveChanged();
}

double CoverExportSession::chartFrameDiskDiameter() const
{
    return chartFrameAvailable_ && frameRenderer_ != nullptr
        ? frameRenderer_->playfieldDiskDiameterFraction() : 0.0;
}

bool CoverExportSession::containsDifficulty(int difficultyId) const
{
    for (const QVariant& row : difficulties_) {
        if (row.toMap().value(QStringLiteral("id")).toInt() == difficultyId) {
            return true;
        }
    }
    return false;
}

int CoverExportSession::defaultDifficultyId(int preferredDifficultyId) const
{
    if (containsDifficulty(preferredDifficultyId)) {
        return preferredDifficultyId;
    }
    if (containsDifficulty(selectedDifficultyId_)) {
        return selectedDifficultyId_;
    }
    return difficulties_.isEmpty() ? 0 : difficulties_.constFirst().toMap().value(QStringLiteral("id")).toInt();
}

void CoverExportSession::rebuildDifficultyList()
{
    const QVariantList next = difficultyOptions(exportEngine_.difficultyIds());
    if (difficulties_ != next) {
        difficulties_ = next;
        emit difficultiesChanged();
    }
}

void CoverExportSession::selectDifficulty(int difficultyId)
{
    const int next = containsDifficulty(difficultyId) ? difficultyId : 0;
    if (selectedDifficultyId_ == next) {
        return;
    }
    selectedDifficultyId_ = next;
    emit selectedDifficultyIdChanged();
    if (pageSessionActive_ && next > 0) {
        seedFromDifficulty(next);
    }
}

void CoverExportSession::seedFromDifficulty(int difficultyId)
{
    commitActiveLayerFrameSeconds();
    stopAndDetachLiveChartScene();
    setBusy(true);
    const VideoExportTask nextTask = exportEngine_.buildSeedTask(difficultyId);
    const bool chartChanged = task_.chartPath != nextTask.chartPath;
    if (chartChanged) flushComposition();
    task_ = nextTask;
    if (chartChanged || outputFile_.isEmpty()) {
        const QJsonObject project = miacode::project_preferences::load(task_.chartPath);
        outputFile_ = project.value(QStringLiteral("coverExport")).toObject()
            .value(QStringLiteral("outputFile")).toString(
                QString::fromLatin1(miacode::cover_export::CoverCompositionState::kDefaultOutputFile));
        if (outputFile_.trimmed().isEmpty()) {
            outputFile_ = QString::fromLatin1(miacode::cover_export::CoverCompositionState::kDefaultOutputFile);
        }
        outputDirty_ = false;
    }
    if (!hasLoadedPreferences_) {
        for (int index = 0; index < std::size(kCoverResolutionPresets); ++index) {
            const auto& preset = kCoverResolutionPresets[index];
            if (preset.width == task_.outputWidth && preset.height == task_.outputHeight) {
                resolutionIndex_ = index;
                break;
            }
        }
    }
    frameRenderer_ = std::make_unique<miacode::cover_export::SceneFrameRenderer>();
    chartFrameAvailable_ = !task_.noteMarkers.isEmpty() && frameRenderer_->bootstrap(task_);
    chartFrameDuration_ = chartFrameAvailable_ ? frameRenderer_->contentDurationSeconds() : 0.0;

    if (!hasLoadedPreferences_) {
        QJsonObject saved = miacode::app_preferences::coverExportPreferences().loadPreferences();
        saved.remove(QStringLiteral("outputFile"));
        saved.remove(QStringLiteral("output"));
        if (!saved.isEmpty()) {
            applyCompositionJson(saved, false);
        }
        hasLoadedPreferences_ = true;
    }
    // The layout survives difficulty changes, but a still belongs to the
    // chart that produced it. Drop those secondary images before installing a
    // new live frame state; the active layer will be painted by the live scene
    // immediately, while a later capture may repopulate inactive layers.
    for (auto* layer : layout_->chartFrameLayers()) {
        layout_->clearLayerImage(layer->key());
    }
    if (!chartFrameAvailable_) {
        for (auto* layer : layout_->chartFrameLayers()) {
            layer->setVisible(false);
        }
    }
    // v1 makes the first visible chart frame the live frame when entering the
    // cover editor. The visible Quick scene is the primary preview; a still
    // capture is never allowed to decide whether the page is usable.
    const auto visibleChartFrames = layout_->visibleChartFrameLayers();
    const QString nextActiveLayerKey = chartFrameAvailable_ && !visibleChartFrames.isEmpty()
        ? visibleChartFrames.constFirst()->key()
        : miacode::cover_export::CoverLayoutModel::cardKey();
    if (activeLayerKey_ != nextActiveLayerKey) {
        activeLayerKey_ = nextActiveLayerKey;
        emit activeLayerChanged();
    }
    if (activeCoverLayer() == nullptr) {
        activeLayerKey_ = miacode::cover_export::CoverLayoutModel::cardKey();
        emit activeLayerChanged();
    }
    emit outputChanged();
    emit chartFrameAvailabilityChanged();
    emit inputsChanged();
    syncPlaybackFromActiveLayer();
    // The cover window is constructed before the session is seeded. Restoring a saved
    // frame can leave playback_->seconds() unchanged during this final sync,
    // so no secondsChanged signal would be emitted for the existing QML
    // binding. Republish the settled layer time after the active layer and
    // duration are both final.
    emit activeChartFrameSecondsChanged();
    rebindLiveChartScene();
    // Warm the secondary capture surface without making it a prerequisite for
    // the live preview. By the time the user switches away from the active
    // chart frame, the surface has had normal event-loop time to initialize.
    if (chartFrameAvailable_ && !visibleChartFrames.isEmpty() && frameRenderer_ != nullptr) {
        frameRenderer_->prepareCaptureWindow(
            qBound(512, qMax(outputWidth(), outputHeight()), 2048),
            activeChartFrameSeconds());
    }
    setBusy(false);
}

miacode::cover_export::CoverLayer* CoverExportSession::activeCoverLayer() const
{
    return layout_ != nullptr ? layout_->layer(activeLayerKey_) : nullptr;
}

void CoverExportSession::selectLayerKey(const QString& key)
{
    if (layout_ == nullptr || layout_->layer(key) == nullptr || activeLayerKey_ == key) {
        return;
    }
    if (activeCoverLayer() != nullptr && activeCoverLayer()->kind() == QStringLiteral("chartFrame")) {
        playback_->pause();
        playback_->cancelInput();
        commitActiveLayerFrameSeconds();
    }
    activeLayerKey_ = key;
    emit activeLayerChanged();
    syncPlaybackFromActiveLayer();
    rebindLiveChartScene();
    emit activeChartFrameSecondsChanged();
    emit inputsChanged();
}

bool CoverExportSession::renderChartFrame(miacode::cover_export::CoverLayer* layer,
                                             int sidePx, bool reportErrors)
{
    if (layer == nullptr || layer->kind() != QStringLiteral("chartFrame")
        || !chartFrameAvailable_ || frameRenderer_ == nullptr) {
        return false;
    }
    const int side = sidePx > 0 ? sidePx : qBound(512, qMax(outputWidth(), outputHeight()), 2048);
    layer->setFrameSeconds(qBound(0.0, layer->frameSeconds(), chartFrameDuration_));
    const double previousPlayhead = frameRenderer_->playheadSeconds();
    const bool captureWasReady = frameRenderer_->captureReady();
    QString error;
    const QImage image = frameRenderer_->renderAt(layer->frameSeconds(), side, &error);
    frameRenderer_->setPlayheadSeconds(previousPlayhead);
    if (auto* liveScene = qobject_cast<PreviewQuickSceneRoot*>(sceneBinder_->liveChartScene())) {
        liveScene->update();
    }
    if (image.isNull()) {
        // A newly-created Quick window needs one or more event-loop turns to
        // become exposed and initialize its scene graph. This is a normal
        // transient state for the preview path, not a user-visible render error.
        if (!captureWasReady && !frameRenderer_->captureReady()) {
            return false;
        }
        if (reportErrors) {
            notifyError(miacode::localizedText("cover.chart_frame"),
                        miacode::localizedText("cover.could_not_render_the_chart"), error);
        }
        return false;
    }
    layout_->setLayerImage(layer->key(), image);
    return true;
}

void CoverExportSession::syncPlaybackFromActiveLayer()
{
    if (playback_ == nullptr) {
        return;
    }
    auto* layer = activeCoverLayer();
    const bool chartFrame = isActiveChartFrame(layer);
    playback_->setDuration(chartFrame && chartFrameAvailable_ ? chartFrameDuration_ : 0.0);
    playback_->setSeconds(chartFrame ? qBound(0.0, layer->frameSeconds(), chartFrameDuration_) : 0.0);
    if (chartFrame && layer->frameSeconds() != playback_->seconds()) {
        layer->setFrameSeconds(playback_->seconds());
    }
    if (chartFrame && frameRenderer_ != nullptr) {
        frameRenderer_->setPlayheadSeconds(playback_->seconds());
    }
    if (!chartFrame) {
        playback_->pause();
        playback_->cancelInput();
    }
}

void CoverExportSession::onPlaybackSecondsChanged()
{
    auto* layer = activeCoverLayer();
    if (!isActiveChartFrame(layer) || playback_ == nullptr) {
        return;
    }
    const double seconds = qBound(0.0, playback_->seconds(), chartFrameDuration_);
    layer->setFrameSeconds(seconds);
    if (frameRenderer_ != nullptr) {
        frameRenderer_->setPlayheadSeconds(seconds);
    }
    if (auto* liveScene = qobject_cast<PreviewQuickSceneRoot*>(sceneBinder_->liveChartScene())) {
        liveScene->update();
    }
    emit activeChartFrameSecondsChanged();
}

void CoverExportSession::onPlaybackReachedEnd()
{
    commitActiveLayerFrameSeconds();
}

bool CoverExportSession::renderVisibleChartFramesForExport(int sidePx)
{
    if (!chartFrameAvailable_ || layout_ == nullptr) {
        return true;
    }
    const auto plan = miacode::cover_export::CoverFrameExportPlan::fromVisibleLayers(
        *layout_, activeLayerKey_, activeChartFrameSeconds());
    for (const auto& frame : plan.frames()) {
        auto* layer = layout_->layer(frame.key);
        if (layer == nullptr) {
            return false;
        }
        layer->setFrameSeconds(frame.seconds);
        if (!renderChartFrame(layer, sidePx, true)) {
            return false;
        }
    }
    return true;
}

void CoverExportSession::rebindLiveChartScene()
{
    if (lastLiveChartScene_.isNull()) {
        return;
    }
    auto* layer = activeCoverLayer();
    if (!isActiveChartFrame(layer) || !layer->visible() || !chartFrameAvailable_) {
        if (auto* liveScene = qobject_cast<PreviewQuickSceneRoot*>(sceneBinder_->liveChartScene())) {
            liveScene->setFrameState(nullptr);
        }
        sceneBinder_->detachLiveChartScene();
        return;
    }
    bindLiveChartScene(lastLiveChartScene_.data());
}

void CoverExportSession::stopAndDetachLiveChartScene()
{
    if (playback_ != nullptr) {
        playback_->pause();
        playback_->cancelInput();
    }
    if (sceneBinder_ != nullptr) {
        if (auto* liveScene = qobject_cast<PreviewQuickSceneRoot*>(lastLiveChartScene_.data())) {
            liveScene->setFrameState(nullptr);
        }
        sceneBinder_->detachLiveChartScene();
    }
}

bool CoverExportSession::isActiveChartFrame(
    const miacode::cover_export::CoverLayer* layer) const
{
    return layer != nullptr && layer->kind() == QStringLiteral("chartFrame")
        && activeLayerKey_ == layer->key();
}

void CoverExportSession::addChartFrameLayer()
{
    if (!chartFrameAvailable_ || layout_ == nullptr) {
        notifyError(miacode::localizedText("cover.chart_frame"),
                    miacode::localizedText("cover.this_difficulty_has_no_chart"));
        return;
    }
    auto* layer = layout_->addChartFrameLayer(frameRenderer_ != nullptr ? frameRenderer_->playheadSeconds() : 0.0);
    if (layer == nullptr) {
        return;
    }
    selectLayerKey(layer->key());
    renderChartFrame(layer);
    persistComposition();
}

void CoverExportSession::addImageLayer()
{
    miacode::FileRequest request;
    request.title = miacode::localizedText("cover.choose_image");
    request.nameFilters = {miacode::localizedText("cover.images_png_jpg_jpeg_bmp")};
    uiRequests_->requestFile(request, [this](const QString& path) {
        if (path.isEmpty() || layout_ == nullptr) return;
        if (auto* layer = layout_->addImageLayer(path)) {
            selectLayerKey(layer->key());
            persistComposition();
        }
    });
}

void CoverExportSession::addTextLayer()
{
    if (layout_ == nullptr) return;
    if (auto* layer = layout_->addTextLayer(qtTrId("cover.text_layer_default"))) {
        selectLayerKey(layer->key());
        persistComposition();
    }
}

void CoverExportSession::duplicateActiveLayer()
{
    if (layout_ == nullptr) return;
    if (auto* layer = layout_->duplicateLayer(activeLayerKey_)) {
        selectLayerKey(layer->key());
        if (layer->kind() == QStringLiteral("chartFrame")) {
            renderChartFrame(layer);
        }
        persistComposition();
    }
}

void CoverExportSession::removeActiveLayer()
{
    if (layout_ == nullptr || activeLayerKey_ == miacode::cover_export::CoverLayoutModel::cardKey()) return;
    if (isActiveChartFrame(activeCoverLayer())) {
        playback_->pause();
        playback_->cancelInput();
        commitActiveLayerFrameSeconds();
    }
    const QString next = layout_->selectionAfterRemoval(activeLayerKey_);
    if (layout_->removeLayer(activeLayerKey_)) {
        activeLayerKey_ = layout_->layer(next) != nullptr ? next : miacode::cover_export::CoverLayoutModel::cardKey();
        emit activeLayerChanged();
        syncPlaybackFromActiveLayer();
        rebindLiveChartScene();
        emit activeChartFrameSecondsChanged();
        persistComposition();
    }
}

void CoverExportSession::bringActiveLayerToFront()
{
    if (layout_ == nullptr) return;
    layout_->bringToFront(layout_->indexOfKey(activeLayerKey_));
    persistComposition();
}

void CoverExportSession::sendActiveLayerToBack()
{
    if (layout_ == nullptr) return;
    layout_->sendToBack(layout_->indexOfKey(activeLayerKey_));
    persistComposition();
}

void CoverExportSession::raiseActiveLayer()
{
    if (layout_ == nullptr) return;
    layout_->raiseLayer(layout_->indexOfKey(activeLayerKey_));
    persistComposition();
}

void CoverExportSession::lowerActiveLayer()
{
    if (layout_ == nullptr) return;
    layout_->lowerLayer(layout_->indexOfKey(activeLayerKey_));
    persistComposition();
}

void CoverExportSession::moveLayer(const QString& key, int viewRow)
{
    if (layout_ == nullptr || busy_) return;
    auto ordered = layout_->layers();
    std::sort(ordered.begin(), ordered.end(), [](const auto* a, const auto* b) {
        return a->z() > b->z();
    });
    for (int i = 0; i < ordered.size(); ++i) {
        if (ordered[i]->key() == key) {
            layout_->moveByViewRows(i, viewRow);
            persistComposition();
            return;
        }
    }
}

void CoverExportSession::browseActiveLayerImage()
{
    auto* layer = activeCoverLayer();
    if (layer == nullptr || layer->kind() != QStringLiteral("image")) return;
    miacode::FileRequest request;
    request.title = miacode::localizedText("cover.choose_image");
    request.startPath = QFileInfo(layer->imagePath()).absolutePath();
    request.nameFilters = {miacode::localizedText("cover.images_png_jpg_jpeg_bmp")};
    uiRequests_->requestFile(request, [this](const QString& path) {
        if (path.isEmpty()) return;
        if (auto* active = activeCoverLayer(); active != nullptr && active->kind() == QStringLiteral("image")) {
            active->setImagePath(path);
            persistComposition();
        }
    });
}

void CoverExportSession::requestFont(bool displayFont, bool textLayerFont)
{
    miacode::FileRequest request;
    request.title = miacode::localizedText("card_font.import");
    request.nameFilters = {miacode::localizedText("file_filter.font")};
    uiRequests_->requestFile(request, [this, displayFont, textLayerFont](const QString& path) {
        if (path.isEmpty()) return;
        const auto result = miacode::video_export::importFontFileIntoLibrary(path, miacode::app_preferences::fontLibraryDirectory());
        if (result.path.isEmpty()) {
            notifyError(miacode::localizedText("card_font.import"),
                        miacode::localizedText(result.failure == miacode::video_export::FontImportFailure::CopyFailed ? "card_font.copy_failed" : "card_font.invalid_font"));
            return;
        }
        if (textLayerFont) {
            if (auto* layer = activeCoverLayer(); layer != nullptr && layer->kind() == QStringLiteral("text")) {
                layer->setFontPath(result.path);
                persistComposition();
            }
        } else if (displayFont) {
            setCardFontDisplayPath(result.path);
        } else {
            setCardFontBodyPath(result.path);
        }
        emit fontLibraryChanged();
    });
}

void CoverExportSession::importActiveLayerFont() { requestFont(false, true); }
void CoverExportSession::importCardDisplayFont() { requestFont(true, false); }
void CoverExportSession::importCardBodyFont() { requestFont(false, false); }

void CoverExportSession::setActiveLayerVisible(bool visible)
{
    setLayerVisible(activeLayerKey_, visible);
}

void CoverExportSession::setLayerVisible(const QString& key, bool visible)
{
    if (auto* layer = layout_ != nullptr ? layout_->layer(key) : nullptr) {
        layer->setVisible(visible);
        if (isActiveChartFrame(layer)) {
            if (!visible) {
                playback_->pause();
                playback_->cancelInput();
            } else {
                renderChartFrame(layer);
            }
            syncPlaybackFromActiveLayer();
            rebindLiveChartScene();
        }
        persistComposition();
    }
}
void CoverExportSession::setActiveLayerLocked(bool locked)
{
    setLayerLocked(activeLayerKey_, locked);
}

void CoverExportSession::setLayerLocked(const QString& key, bool locked)
{
    if (auto* layer = layout_ != nullptr ? layout_->layer(key) : nullptr) {
        layer->setLocked(locked);
        persistComposition();
    }
}
void CoverExportSession::setActiveLayerOpacity(double opacity)
{
    if (auto* layer = activeCoverLayer()) { layer->setOpacity(qBound(0.0, opacity, 1.0)); persistComposition(); }
}
void CoverExportSession::setActiveLayerSizeFraction(double sizeFraction)
{
    if (auto* layer = activeCoverLayer()) { layer->setSizeFraction(qBound(0.05, sizeFraction, 2.0)); persistComposition(); }
}
void CoverExportSession::setActiveLayerCenter(double nx, double ny)
{
    if (auto* layer = activeCoverLayer()) {
        layer->setNx(qBound(0.0, nx, 1.0));
        layer->setNy(qBound(0.0, ny, 1.0));
        persistComposition();
    }
}
void CoverExportSession::setActiveLayerText(const QString& text)
{
    if (auto* layer = activeCoverLayer(); layer != nullptr && layer->kind() == QStringLiteral("text")) {
        layer->setText(text); persistComposition();
    }
}
void CoverExportSession::setActiveLayerTextColor(const QString& color)
{
    if (auto* layer = activeCoverLayer(); layer != nullptr && layer->kind() == QStringLiteral("text")) {
        layer->setTextColor(color); persistComposition();
    }
}
void CoverExportSession::setActiveLayerTextBold(bool bold)
{
    if (auto* layer = activeCoverLayer(); layer != nullptr && layer->kind() == QStringLiteral("text")) {
        layer->setTextBold(bold); persistComposition();
    }
}
void CoverExportSession::setActiveLayerFrameSeconds(double seconds)
{
    previewActiveLayerFrameSeconds(seconds);
    commitActiveLayerFrameSeconds();
}

void CoverExportSession::previewActiveLayerFrameSeconds(double seconds)
{
    auto* layer = activeCoverLayer();
    if (!isActiveChartFrame(layer) || playback_ == nullptr) {
        return;
    }
    playback_->setSeconds(qBound(0.0, seconds, chartFrameDuration_));
    // `setSeconds` emits synchronously when the value changes. Keep this
    // explicit path for a no-op drag at the same value as well.
    onPlaybackSecondsChanged();
}

void CoverExportSession::commitActiveLayerFrameSeconds()
{
    auto* layer = activeCoverLayer();
    if (!isActiveChartFrame(layer)) {
        return;
    }
    layer->setFrameSeconds(qBound(0.0, layer->frameSeconds(), chartFrameDuration_));
    renderChartFrame(layer);
    persistComposition();
    emit activeChartFrameSecondsChanged();
}

void CoverExportSession::toggleActiveLayerPlayback()
{
    if (!isActiveChartFrame(activeCoverLayer()) || playback_ == nullptr || !chartFrameAvailable_) {
        return;
    }
    syncPlaybackFromActiveLayer();
    playback_->toggle();
}

void CoverExportSession::beginActiveLayerKeySeek(int direction)
{
    if (!isActiveChartFrame(activeCoverLayer()) || playback_ == nullptr || !chartFrameAvailable_) {
        return;
    }
    syncPlaybackFromActiveLayer();
    playback_->beginKeySeek(direction);
}

void CoverExportSession::endActiveLayerKeySeek()
{
    if (playback_ == nullptr) {
        return;
    }
    playback_->endKeySeek();
    commitActiveLayerFrameSeconds();
}

void CoverExportSession::cancelActiveLayerInput()
{
    if (playback_ == nullptr) {
        return;
    }
    playback_->pause();
    playback_->cancelInput();
}

void CoverExportSession::commitActiveLayerGeometry()
{
    persistComposition();
}

void CoverExportSession::commitCompositionChanges()
{
    persistComposition();
}

void CoverExportSession::bindLiveChartScene(QObject* scene)
{
    auto* liveScene = qobject_cast<PreviewQuickSceneRoot*>(scene);
    if (liveScene == nullptr) {
        if (scene != nullptr) {
            return;
        }
        if (sceneBinder_ != nullptr) {
            if (auto* current = qobject_cast<PreviewQuickSceneRoot*>(sceneBinder_->liveChartScene())) {
                current->setFrameState(nullptr);
            }
            sceneBinder_->detachLiveChartScene();
        }
        lastLiveChartScene_.clear();
        return;
    }
    lastLiveChartScene_ = liveScene;
    liveScene->setLayerFlags(miacode::preview::scene::kPreviewExportOverlayRenderLayers);
    liveScene->setFrameState(
        frameRenderer_ != nullptr && chartFrameAvailable_ ? frameRenderer_->frameState() : nullptr);
    sceneBinder_->setFrameState(
        frameRenderer_ != nullptr && chartFrameAvailable_ ? frameRenderer_->frameState() : nullptr);
    sceneBinder_->bindLiveChartScene(liveScene);
}

void CoverExportSession::unbindLiveChartScene(QObject* scene)
{
    if (scene == nullptr || sceneBinder_ == nullptr) {
        return;
    }
    if (auto* liveScene = qobject_cast<PreviewQuickSceneRoot*>(scene)) {
        liveScene->setFrameState(nullptr);
    }
    sceneBinder_->unbindLiveChartScene(scene);
    if (lastLiveChartScene_.data() == scene) {
        lastLiveChartScene_.clear();
    }
}
void CoverExportSession::setActiveLayerFrameBackgroundMode(const QString& mode)
{
    if (auto* layer = activeCoverLayer(); layer != nullptr && layer->kind() == QStringLiteral("chartFrame")) {
        layer->setFrameBgMode(mode == QStringLiteral("transparent") ? mode : QStringLiteral("image"));
        persistComposition();
    }
}
void CoverExportSession::setActiveLayerFrameBackgroundBrightness(double brightness)
{
    if (auto* layer = activeCoverLayer(); layer != nullptr && layer->kind() == QStringLiteral("chartFrame")) {
        layer->setFrameBgBrightness(qBound(0.0, brightness, 1.0)); persistComposition();
    }
}
void CoverExportSession::setActiveLayerFrameBackgroundTransparency(double transparency)
{
    if (auto* layer = activeCoverLayer(); layer != nullptr && layer->kind() == QStringLiteral("chartFrame")) {
        layer->setFrameBgTransparency(qBound(0.0, transparency, 1.0)); persistComposition();
    }
}

void CoverExportSession::setBackgroundMode(int mode)
{
    const auto next = static_cast<miacode::cover_export::CoverBackgroundMode>(qBound(0, mode, 2));
    if (backgroundMode_ == next) return;
    backgroundMode_ = next; emit inputsChanged(); persistComposition();
}
void CoverExportSession::setBlurBackground(bool enabled)
{
    if (blurBackground_ == enabled) return;
    blurBackground_ = enabled; emit inputsChanged(); persistComposition();
}
void CoverExportSession::setBackgroundBrightness(double value)
{
    const double next = qBound(0.0, value, 1.0);
    if (qFuzzyCompare(backgroundBrightness_, next)) return;
    backgroundBrightness_ = next; emit inputsChanged(); persistComposition();
}
void CoverExportSession::setCardShadow(bool enabled)
{
    if (cardShadow_ == enabled) return;
    cardShadow_ = enabled; emit inputsChanged(); persistComposition();
}
void CoverExportSession::setCardMode(const QString& mode)
{
    const QString next = isAutoIntroBannerMode(mode)
        ? QStringLiteral("auto") : normalizedIntroBannerMode(mode);
    if (cardMode_ == next) return;
    cardMode_ = next; emit inputsChanged(); persistComposition();
}
void CoverExportSession::setLevelTextRender(bool enabled)
{
    if (levelTextRender_ == enabled) return;
    levelTextRender_ = enabled; emit inputsChanged(); persistComposition();
}
void CoverExportSession::setLongTextMode(const QString& mode)
{
    const QString next = mode == QStringLiteral("ellipsis") ? mode : QStringLiteral("shrink");
    if (longTextMode_ == next) return;
    longTextMode_ = next; emit inputsChanged(); persistComposition();
}
void CoverExportSession::setCardFontDisplayPath(const QString& path)
{
    if (cardFontDisplayPath_ == path) return;
    cardFontDisplayPath_ = path; emit inputsChanged(); persistComposition();
}
void CoverExportSession::setCardFontBodyPath(const QString& path)
{
    if (cardFontBodyPath_ == path) return;
    cardFontBodyPath_ = path; emit inputsChanged(); persistComposition();
}
void CoverExportSession::setResolutionIndex(int index)
{
    const int next = qBound(0, index, static_cast<int>(std::size(kCoverResolutionPresets)) - 1);
    if (resolutionIndex_ == next) return;
    resolutionIndex_ = next; emit outputChanged(); persistComposition();
}
void CoverExportSession::setOutputFile(const QString& path)
{
    QString input = path.trimmed();
    if (input.isEmpty()) return;
#ifndef Q_OS_WIN
    if (input.startsWith(QStringLiteral("~/"))) {
        input = QDir::homePath() + input.mid(1);
    }
#endif
    // A folder has no file name to write to; it is not a usable answer here.
    if (input.endsWith(QLatin1Char('/')) || input.endsWith(QLatin1Char('\\'))) return;
    const QString next = QDir::cleanPath(input);
    if (outputFile_ == next) return;
    outputFile_ = next;
    outputDirty_ = true;
    emit outputChanged();
    compositionSaveTimer_.start();
}

QString CoverExportSession::outputFilePath() const
{
    if (outputFile_.isEmpty()) return {};
    // A relative file is read against the chart folder, never against the
    // process working directory.
    return QDir::cleanPath(QDir(coverOutputBaseDirectory(task_.chartPath)).absoluteFilePath(outputFile_));
}

void CoverExportSession::browseBackgroundImage()
{
    miacode::FileRequest request;
    request.title = miacode::localizedText("cover.choose_background_image");
    request.startPath = QFileInfo(backgroundPath_).absolutePath();
    request.nameFilters = {miacode::localizedText("cover.images_png_jpg_jpeg_bmp")};
    uiRequests_->requestFile(request, [this](const QString& path) {
        if (path.isEmpty()) return;
        backgroundPath_ = path;
        backgroundMode_ = miacode::cover_export::CoverBackgroundMode::Custom;
        emit inputsChanged();
        persistComposition();
    });
}

void CoverExportSession::resetLayout()
{
    if (layout_ == nullptr || uiRequests_ == nullptr) return;
    // Reset throws away every layer and every position, so it asks first — the
    // same question v1's 布局 ▾ menu asked before it.
    uiRequests_->requestConfirmation(
        miacode::localizedText("cover.reset_layout"),
        miacode::localizedText("cover.reset_discards_all_current_layers"),
        miacode::localizedText("cover.reset_layout"),
        [this](bool accepted) {
            if (!accepted || layout_ == nullptr) return;
            layout_->resetLayout();
            // The default layout carries a chart frame; a difficulty with no
            // renderable notes must not get it back through the reset.
            if (!chartFrameAvailable_) {
                for (auto* layer : layout_->chartFrameLayers()) {
                    layer->setVisible(false);
                }
            }
            activeLayerKey_ = miacode::cover_export::CoverLayoutModel::cardKey();
            emit activeLayerChanged();
            persistComposition();
        });
}

QJsonObject CoverExportSession::compositionJson() const
{
    miacode::cover_export::CoverCompositionState state;
    state.size = QSize(outputWidth(), outputHeight());
    state.background = {{QStringLiteral("mode"), backgroundMode_ == miacode::cover_export::CoverBackgroundMode::Custom ? QStringLiteral("custom")
                          : backgroundMode_ == miacode::cover_export::CoverBackgroundMode::Transparent ? QStringLiteral("transparent")
                                                                                                           : QStringLiteral("jacket")},
                        {QStringLiteral("customPath"), backgroundPath_},
                        {QStringLiteral("blur"), blurBackground_},
                        {QStringLiteral("brightness"), backgroundBrightness_}};
    state.card = {{QStringLiteral("mode"), cardMode_},
                  {QStringLiteral("shadow"), cardShadow_},
                  {QStringLiteral("levelTextRender"), levelTextRender_},
                  {QStringLiteral("longText"), longTextMode_},
                  {QStringLiteral("fontDisplay"), cardFontDisplayPath_},
                  {QStringLiteral("fontBody"), cardFontBodyPath_}};
    state.layout = layout_ != nullptr ? layout_->toJson() : QJsonObject{};
    state.outputFile = outputFile_;
    return state.toJson();
}

QJsonObject CoverExportSession::sharedCompositionJson() const
{
    QJsonObject root = compositionJson();
    // Output destinations belong to the chart project; shared layouts keep appearance only.
    root.remove(QStringLiteral("outputFile"));
    return root;
}

QJsonObject CoverExportSession::presetCompositionJson() const
{
    QJsonObject root = sharedCompositionJson();
    // A preset also keeps the current canvas rather than carrying its own.
    root.remove(QStringLiteral("size"));
    return root;
}

QJsonObject CoverExportSession::builtinPresetComposition(const QString& id) const
{
    if (id != QStringLiteral("card")
        && id != QStringLiteral("card_chart_frame")
        && id != QStringLiteral("dual_chart_frames")
        && id != QStringLiteral("pure_chart_frame")) {
        return {};
    }

    miacode::cover_export::CoverLayoutModel model;
    auto* card = model.layer(miacode::cover_export::CoverLayoutModel::cardKey());
    if (card == nullptr) {
        return {};
    }
    auto setGeometry = [](miacode::cover_export::CoverLayer* layer,
                          double nx, double ny, double size, int z, bool visible) {
        if (layer == nullptr) return;
        layer->setNx(nx);
        layer->setNy(ny);
        layer->setSizeFraction(size);
        layer->setZ(z);
        layer->setVisible(visible);
    };

    setGeometry(card, 0.5, 0.5, 0.85, 1, true);
    if (id == QStringLiteral("card")) {
        model.normalizeZOrder();
    } else if (id == QStringLiteral("card_chart_frame")) {
        auto* frame = model.addChartFrameLayer(0.0);
        setGeometry(card, 0.64, 0.5, 0.78, 1, true);
        setGeometry(frame, 0.32, 0.5, 0.82, 0, true);
        model.normalizeZOrder();
    } else if (id == QStringLiteral("dual_chart_frames")) {
        auto* first = model.addChartFrameLayer(0.0);
        auto* second = model.addChartFrameLayer(0.0);
        setGeometry(card, 0.5, 0.5, 0.85, 0, false);
        setGeometry(first, 0.30, 0.40, 0.56, 1, true);
        setGeometry(second, 0.66, 0.60, 0.56, 0, true);
        model.normalizeZOrder();
    } else {
        auto* frame = model.addChartFrameLayer(0.0);
        setGeometry(card, 0.5, 0.5, 0.85, 0, false);
        setGeometry(frame, 0.5, 0.5, 0.92, 1, true);
        model.normalizeZOrder();
    }

    QJsonObject root = presetCompositionJson();
    root.remove(QStringLiteral("size"));
    root.insert(QStringLiteral("layout"), model.toJson());
    return root;
}

bool CoverExportSession::applyCompositionJsonInternal(const QJsonObject& root,
                                                         bool reportErrors,
                                                         bool renderChartFrames)
{
    miacode::cover_export::CoverCompositionState state;
    QString error;
    if (!miacode::cover_export::CoverCompositionState::fromJson(root, &state, &error)) {
        if (reportErrors) notifyError(miacode::localizedText("cover.import_layout_2"),
                                      miacode::localizedText("cover.the_layout_file_is_not"), error);
        return false;
    }
    const QSize size = state.size;
    for (int index = 0; index < std::size(kCoverResolutionPresets); ++index) {
        if (QSize(kCoverResolutionPresets[index].width, kCoverResolutionPresets[index].height) == size) {
            resolutionIndex_ = index;
            break;
        }
    }
    const QJsonObject background = state.background;
    const QString mode = background.value(QStringLiteral("mode")).toString(QStringLiteral("jacket"));
    backgroundMode_ = mode == QStringLiteral("custom") ? miacode::cover_export::CoverBackgroundMode::Custom
                    : mode == QStringLiteral("transparent") ? miacode::cover_export::CoverBackgroundMode::Transparent
                                                              : miacode::cover_export::CoverBackgroundMode::Jacket;
    backgroundPath_ = background.value(QStringLiteral("customPath")).toString();
    if (backgroundMode_ == miacode::cover_export::CoverBackgroundMode::Custom
        && !QFileInfo::exists(backgroundPath_)) {
        backgroundMode_ = miacode::cover_export::CoverBackgroundMode::Jacket;
        if (reportErrors) {
            notifyError(miacode::localizedText("cover.background"),
                        miacode::localizedText("cover.the_custom_background_image_was"));
        }
    }
    blurBackground_ = background.value(QStringLiteral("blur")).toBool(true);
    backgroundBrightness_ = qBound(0.0, background.value(QStringLiteral("brightness")).toDouble(0.45), 1.0);
    const QJsonObject card = state.card;
    cardMode_ = card.value(QStringLiteral("mode")).toString(QStringLiteral("auto"));
    cardShadow_ = card.value(QStringLiteral("shadow")).toBool(false);
    levelTextRender_ = card.value(QStringLiteral("levelTextRender")).toBool(false);
    longTextMode_ = card.value(QStringLiteral("longText")).toString(QStringLiteral("shrink"));
    cardFontDisplayPath_ = card.value(QStringLiteral("fontDisplay")).toString();
    cardFontBodyPath_ = card.value(QStringLiteral("fontBody")).toString();
    // Presets and older layouts carry no output file; keep the current one
    // rather than blanking the field.
    if (const QString savedOutput = state.outputFile.trimmed(); !savedOutput.isEmpty()) {
        if (outputFile_ != savedOutput) {
            outputFile_ = savedOutput;
            outputDirty_ = true;
        }
    }
    if (layout_ != nullptr) {
        layout_->fromJson(state.layout);
        for (auto* frame : layout_->chartFrameLayers()) {
            layout_->clearLayerImage(frame->key());
        }
        if (!chartFrameAvailable_) {
            for (auto* frame : layout_->chartFrameLayers()) {
                frame->setVisible(false);
            }
        } else if (renderChartFrames) {
            bool framesReady = true;
            for (auto* frame : layout_->visibleChartFrameLayers()) {
                framesReady = renderChartFrame(frame, 0, reportErrors) && framesReady;
            }
            if (!framesReady) {
                return false;
            }
        }
        const auto visibleChartFrames = layout_->visibleChartFrameLayers();
        const QString firstVisibleFrameKey = !visibleChartFrames.isEmpty()
            ? visibleChartFrames.constFirst()->key() : QString();
        if (!firstVisibleFrameKey.isEmpty()) {
            activeLayerKey_ = firstVisibleFrameKey;
        } else {
            activeLayerKey_ = miacode::cover_export::CoverLayoutModel::cardKey();
        }
    } else {
        activeLayerKey_ = miacode::cover_export::CoverLayoutModel::cardKey();
    }
    if (playback_ != nullptr) {
        playback_->pause();
        playback_->cancelInput();
    }
    emit activeLayerChanged();
    syncPlaybackFromActiveLayer();
    rebindLiveChartScene();
    emit activeChartFrameSecondsChanged();
    emit inputsChanged();
    emit outputChanged();
    return true;
}

bool CoverExportSession::applyCompositionJson(const QJsonObject& root, bool reportErrors)
{
    // Applying a layout also swaps the live chart-frame stills. Keep the old
    // composition and images until every new frame has rendered successfully;
    // a transient Quick capture failure must not leave the editor half-applied.
    const QJsonObject previous = compositionJson();
    const QString previousActiveLayerKey = activeLayerKey_;
    QHash<QString, QImage> previousFrameImages;
    if (layout_ != nullptr) {
        for (auto* frame : layout_->chartFrameLayers()) {
            previousFrameImages.insert(frame->key(), frame->frameImage());
        }
    }

    if (applyCompositionJsonInternal(root, reportErrors, true)) {
        return true;
    }

    if (!previous.isEmpty()) {
        applyCompositionJsonInternal(previous, false, false);
        if (layout_ != nullptr) {
            for (auto* frame : layout_->chartFrameLayers()) {
                const QImage image = previousFrameImages.value(frame->key());
                if (!image.isNull()) {
                    layout_->setLayerImage(frame->key(), image);
                }
            }
            if (layout_->layer(previousActiveLayerKey) != nullptr) {
                activeLayerKey_ = previousActiveLayerKey;
                emit activeLayerChanged();
                syncPlaybackFromActiveLayer();
                rebindLiveChartScene();
                emit activeChartFrameSecondsChanged();
            }
        }
    }
    return false;
}

void CoverExportSession::persistComposition()
{
    compositionDirty_ = true;
    compositionSaveTimer_.start();
}

void CoverExportSession::flushComposition()
{
    compositionSaveTimer_.stop();
    if (compositionDirty_
        && miacode::app_preferences::coverExportPreferences().savePreferences(sharedCompositionJson())) {
        compositionDirty_ = false;
    }
    if (outputDirty_ && !task_.chartPath.isEmpty()) {
        QJsonObject project = miacode::project_preferences::load(task_.chartPath);
        QJsonObject cover = project.value(QStringLiteral("coverExport")).toObject();
        cover.insert(QStringLiteral("outputFile"), outputFile_);
        project.insert(QStringLiteral("coverExport"), cover);
        if (miacode::project_preferences::save(task_.chartPath, project)) outputDirty_ = false;
    }
}

void CoverExportSession::saveLayout()
{
    miacode::FileRequest request;
    request.title = miacode::localizedText("cover.save_cover_layout");
    request.startPath = QStringLiteral("cover-layout.miacover");
    request.nameFilters = {miacode::localizedText("cover.cover_layout_miacover")};
    request.saveMode = true;
    uiRequests_->requestFile(request, [this](QString path) {
        if (path.isEmpty()) return;
        if (!path.endsWith(QStringLiteral(".miacover"), Qt::CaseInsensitive)) path += QStringLiteral(".miacover");
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)
            || file.write(QJsonDocument(sharedCompositionJson()).toJson(QJsonDocument::Indented)) < 0) {
            notifyError(miacode::localizedText("cover.save_layout_2"),
                        miacode::localizedText("cover.could_not_write_the_layout"), path);
            return;
        }
        miacode::app_preferences::coverExportPreferences().pushRecentFile(path);
        refreshSavedLists();
    });
}

void CoverExportSession::importLayout()
{
    miacode::FileRequest request;
    request.title = miacode::localizedText("cover.import_cover_layout");
    request.nameFilters = {miacode::localizedText("cover.cover_layout_miacover_legacy_json")};
    uiRequests_->requestFile(request, [this](const QString& path) { openRecentLayout(path); });
}

void CoverExportSession::openRecentLayout(const QString& path)
{
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        notifyError(miacode::localizedText("cover.import_layout_2"),
                    miacode::localizedText("cover.could_not_read_the_layout"), path);
        return;
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject() || document.object().value(QStringLiteral("kind")).toString()
                                    != QStringLiteral("miacode-cover-composition")) {
        notifyError(miacode::localizedText("cover.import_layout_2"),
                    miacode::localizedText("cover.this_file_is_not_a"), path);
        return;
    }
    if (applyCompositionJson(document.object(), true)) {
        miacode::app_preferences::coverExportPreferences().pushRecentFile(path);
        persistComposition();
        refreshSavedLists();
    }
}

void CoverExportSession::refreshSavedLists()
{
    recentLayoutFiles_ = miacode::app_preferences::coverExportPreferences().loadRecentFiles();
    presets_.clear();
    for (const auto& preset : miacode::app_preferences::coverExportPreferences().loadUserPresets()) {
        presets_.append(QVariantMap{{QStringLiteral("name"), preset.name}});
    }
    emit recentLayoutFilesChanged();
    emit presetsChanged();
}

void CoverExportSession::clearRecentLayouts()
{
    miacode::app_preferences::coverExportPreferences().clearRecentFiles();
    refreshSavedLists();
}
void CoverExportSession::savePreset(const QString& name)
{
    miacode::app_preferences::coverExportPreferences().saveUserPreset(name, presetCompositionJson());
    refreshSavedLists();
}

void CoverExportSession::applyBuiltinPreset(const QString& id)
{
    if (id != QStringLiteral("card") && !chartFrameAvailable_) {
        notifyError(miacode::localizedText("cover.apply_preset"),
                    miacode::localizedText("cover.this_preset_needs_a_renderable"));
        return;
    }
    const QJsonObject composition = builtinPresetComposition(id);
    if (!composition.isEmpty() && applyCompositionJson(composition, true)) {
        persistComposition();
    }
}

void CoverExportSession::renamePreset(const QString& oldName, const QString& newName)
{
    const QString trimmedOld = oldName.trimmed();
    const QString trimmedNew = newName.trimmed();
    if (trimmedOld.isEmpty() || trimmedNew.isEmpty() || trimmedOld == trimmedNew) {
        return;
    }
    miacode::app_preferences::coverExportPreferences().renameUserPreset(trimmedOld, trimmedNew);
    refreshSavedLists();
}
void CoverExportSession::applyPreset(const QString& name)
{
    for (const auto& preset : miacode::app_preferences::coverExportPreferences().loadUserPresets()) {
        if (preset.name == name && applyCompositionJson(preset.composition, true)) {
            persistComposition();
            return;
        }
    }
}
void CoverExportSession::removePreset(const QString& name)
{
    miacode::app_preferences::coverExportPreferences().removeUserPreset(name);
    refreshSavedLists();
}

void CoverExportSession::browseOutputFile()
{
    miacode::FileRequest request;
    request.title = miacode::localizedText("cover.export_cover");
    request.startPath = outputFilePath();
    request.nameFilters = {miacode::localizedText("cover.images_png_jpg_jpeg_bmp")};
    request.saveMode = true;
    uiRequests_->requestFile(request, [this](const QString& path) { setOutputFile(path); });
}

miacode::cover_export::CoverComposerInputs CoverExportSession::buildInputs() const
{
    miacode::cover_export::CoverComposerInputs inputs;
    inputs.templateMap = templateMap();
    inputs.trackOverrides = trackOverrides();
    inputs.jacketPath = task_.intro.jacketPath;
    inputs.backgroundPath = backgroundPath_;
    inputs.backgroundMode = backgroundMode_;
    inputs.blurBackground = blurBackground_;
    inputs.coverBgBrightness = backgroundBrightness_;
    inputs.cardShadow = cardShadow_;
    if (const auto* layer = activeCoverLayer(); layer != nullptr && layer->kind() == QStringLiteral("chartFrame")) {
        inputs.chartFrameBackground = layer->frameBgMode() == QStringLiteral("image");
        inputs.chartFrameBgBrightness = layer->frameBgBrightness();
        inputs.chartFrameBgTransparency = layer->frameBgTransparency();
    }
    inputs.chartFrameDiskDiameter = chartFrameDiskDiameter();
    return inputs;
}

void CoverExportSession::exportCover()
{
    if (layout_ == nullptr || busy_) {
        return;
    }
    // v1's onExportCover refused the same way before it ever opened the
    // Widgets cover dialog; the QML route dropped the user-visible half of
    // that guard and just emitted regardless.
    if (!containsDifficulty(selectedDifficultyId_)) {
        notifyError(miacode::localizedText("cover.export_cover"),
                    miacode::localizedText("cover.no_difficulty_selected"));
        return;
    }
    // The coordinator is the single playback authority now; a preview left
    // running under the synchronous render below would keep its audio going
    // under a frozen UI. Only toggle when it is actually playing —
    // togglePlayback() is a toggle, so calling it on a paused/stopped preview
    // would start it instead.
    if (miacode::PlaybackControl* const control = playbackControl(); control != nullptr
        && control->playbackSnapshot().transportState == miacode::PlaybackTransportState::Playing) {
        control->togglePlayback();
    }

    setBusy(true);
    // renderVisibleChartFramesForExport()/exportCoverComposite() below are
    // synchronous in-process QSG work with no progress callback, so nothing
    // yields back to the event loop once they start. Give it one turn here,
    // purely so CoverExportPage's BusyIndicator gets to paint before the
    // render blocks the UI thread.
    QCoreApplication::processEvents();
    const auto plan = miacode::cover_export::CoverFrameExportPlan::fromVisibleLayers(
        *layout_, activeLayerKey_, activeChartFrameSeconds());
    const bool wasPlaying = chartFramePlaying();
    const QString savedActiveKey = plan.activeLayerKey();
    const double savedActiveSeconds = plan.activeLayerSeconds();
    playback_->pause();
    playback_->cancelInput();
    commitActiveLayerFrameSeconds();
    stopAndDetachLiveChartScene();
    const int frameSide = qBound(512, qMax(outputWidth(), outputHeight()), 4096);
    const bool framesReady = renderVisibleChartFramesForExport(frameSide);
    persistComposition();
    const auto result = framesReady
        ? miacode::cover_export::exportCoverComposite(
              layout_.get(), buildInputs(), QSize(outputWidth(), outputHeight()), outputFilePath())
        : miacode::cover_export::CoverExportResult{
              false, QString(), qtTrId("cover.chart_frame_render_failed")};
    if (layout_->layer(savedActiveKey) != nullptr) {
        activeLayerKey_ = savedActiveKey;
        emit activeLayerChanged();
    }
    if (isActiveChartFrame(activeCoverLayer())) {
        previewActiveLayerFrameSeconds(savedActiveSeconds);
        commitActiveLayerFrameSeconds();
    }
    syncPlaybackFromActiveLayer();
    rebindLiveChartScene();
    if (wasPlaying && isActiveChartFrame(activeCoverLayer())) {
        playback_->play();
    }
    setBusy(false);
    if (!result.success) {
        notifyError(miacode::localizedText("cover.export_cover"),
                    miacode::localizedText("cover.cover_export_failed_1").arg(result.errorMessage),
                    result.errorMessage);
        return;
    }
    uiRequests_->postNotice(miacode::NoticeSeverity::Information,
                            miacode::localizedText("cover.export_cover"),
                            miacode::localizedText("cover.cover_export_completed"),
                            result.outputPath);
}

void CoverExportSession::setBusy(bool busy)
{
    if (busy_ == busy) return;
    busy_ = busy;
    emit busyChanged();
}

void CoverExportSession::notifyError(const miacode::LocalizedText& title, const miacode::LocalizedText& text, const miacode::LocalizedText& details) const
{
    if (uiRequests_ != nullptr) {
        uiRequests_->postNotice(miacode::NoticeSeverity::Error, title, text, details);
    }
}

} // namespace miacode::ui
