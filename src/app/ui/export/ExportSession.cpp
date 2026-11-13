#include "app/services/FileDialogPreferences.h"
#include "common/LocalizedText.h"

#include "app/ui/export/ExportSession.h"
#include "app/ui/document/DifficultyOptions.h"

#include "core/chart/document/SimaiDocument.h"

#include "app/services/JobProgressService.h"
#include "app/ui/preferences/LocaleService.h"
#include "core/scene/PreviewSfxAssets.h"
#include "core/video/PreviewRenderSettings.h"
#include "app/services/VideoExportPreferences.h"
#include "export/video_export/VideoExportAudioRenderPlan.h"
#include "export/video_export/VideoExportSettings.h"
#include "export/video_export/FontLibrary.h"
#include "app/services/UserFontLibrary.h"
#include "core/chart/ChartAssetPaths.h"
#include "core/chart/IntroConfig.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QScopedValueRollback>
#include <QSettings>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <utility>


namespace miacode::ui {
namespace {

constexpr auto kBatchOutputDirectoryKey = "last_batch_output_directory";

// v1 kept the batch pickers' history in the native QSettings store. v2 only
// reads it, as the upgrade fallback, and records its own in the dialog
// preferences.
QString legacyBatchExportDirectory(const char* key)
{
    QSettings settings(QStringLiteral("fanfaredash"), QStringLiteral("MiaCode"));
    return miacode::file_dialog_preferences::validDirectory(
        settings.value(QStringLiteral("batch_video_export_dialog/") + QLatin1String(key)).toString());
}

inline constexpr auto& kResolutionPresets = miacode::video_export::kVideoExportResolutionPresets;
inline constexpr auto& kFpsOptions = miacode::video_export::kVideoExportFpsOptions;
inline constexpr auto& kAudioBitrateOptions =
    miacode::video_export::kVideoExportAudioBitrateOptionsKbps;
constexpr double kMinimumExportRangeSeconds = 5.0;

double minimumExportRangeSecondsForChart(double chartDurationSeconds)
{
    return qMin(kMinimumExportRangeSeconds, qMax(0.0, chartDurationSeconds));
}

}  // namespace

ExportSession::ExportSession(miacode::ShellNotifications& notifications,
                                   miacode::UiRequestService& uiRequests,
                                   miacode::JobProgressService& jobProgress,
                                   miacode::PreviewAppearanceState& appearance,
                                   miacode::ExportEngine*& engineSlot,
                                   miacode::PreviewSurface*& previewSlot,
                                   QObject* parent)
    : QObject(parent)
    , uiRequests_(&uiRequests)
    // From the application assembly, not from the hidden window.
    , jobProgress_(&jobProgress)
    , appearance_(&appearance)
    , engineSlot_(&engineSlot)
    , previewSlot_(&previewSlot)
    , notifications_(&notifications)
{
    connect(&miacode::LocaleService::instance(), &miacode::LocaleService::languageChanged,
            this, [this](const QString&) {
                fontLibraryOptionsCacheValid_ = false;
                emit localeLabelsChanged();
                emit fontLibraryChanged();
                emit introSoundOptionsChanged();
                emit introChanged();
            });
    connect(&notifications, &miacode::ShellNotifications::videoExportWorkerRunningChanged, this, [this](bool running) {
        if (batchExportRunning_ || exportRunning_ == running) {
            return;
        }
        exportRunning_ = running;
        emit exportRunningChanged();
    });
    connect(&notifications, &miacode::ShellNotifications::videoExportCompleted, this, [this]() {
        if (!batchExportRunning_ && exportRunning_) {
            resetIntroSelection();
        }
    });
    connect(&notifications, &miacode::ShellNotifications::previewRenderSettingsChanged, this, [this]() {
        if (pageSessionActive_ && !pushingSharedSettings_) {
            adoptPreviewRenderSettings();
        }
    });
    // The audition ends inside the playback coordinator (music faded out,
    // another transport action); it announces that through the presentation.
    connect(&notifications, &miacode::ShellNotifications::presentationChanged,
            this, &ExportSession::introPreviewStateChanged);
    // The size estimate follows everything that feeds planVideoRate.
    connect(this, &ExportSession::outputChanged, this, &ExportSession::estimateChanged);
    connect(this, &ExportSession::rangeChanged, this, &ExportSession::estimateChanged);
    connect(this, &ExportSession::introChanged, this, &ExportSession::estimateChanged);
    connect(this, &ExportSession::rangeChanged, this, [this]() {
        if (rangePlaybackEnabled_ && !hasPendingSelectionRangeExport_) {
            emit playbackRangeRequested(true, exportStartSeconds(), exportEndSeconds());
        }
    });
}

void ExportSession::setRangePlaybackEnabled(bool enabled)
{
    enabled = enabled && rangePreviewAvailable();
    if (rangePlaybackEnabled_ == enabled) {
        return;
    }
    rangePlaybackEnabled_ = enabled;
    emit playbackRangeRequested(enabled, enabled ? exportStartSeconds() : 0.0,
                                enabled ? exportEndSeconds() : 0.0);
    emit rangePlaybackStateChanged();
}

QString ExportSession::activeTab() const
{
    return activeTab_;
}

QVariantList ExportSession::resolutionOptions() const
{
    QVariantList list;
    for (const auto& preset : kResolutionPresets) {
        QVariantMap row;
        row.insert(QStringLiteral("label"), QString::fromLatin1(preset.label));
        row.insert(QStringLiteral("width"), preset.width);
        row.insert(QStringLiteral("height"), preset.height);
        list.append(row);
    }
    return list;
}

QVariantList ExportSession::fpsOptions() const
{
    QVariantList list;
    for (int fps : kFpsOptions) {
        list.append(fps);
    }
    return list;
}

QVariantList ExportSession::audioBitrateOptions() const
{
    QVariantList list;
    for (int kbps : kAudioBitrateOptions) {
        list.append(kbps);
    }
    return list;
}

QVariantList ExportSession::outputModeOptions() const
{
    return {QVariantMap{{"value", 0}, {"label", QStringLiteral("MP4")}},
            QVariantMap{{"value", 1}, {"label", QStringLiteral("WAV")}},
            QVariantMap{{"value", 2}, {"label", QStringLiteral("MP4 + WAV")}}};
}

void ExportSession::setOutputModeIndex(int index)
{
    if (index < 0 || index > 2 || outputModeIndex() == index) {
        return;
    }
    task_.outputMode = static_cast<VideoExportOutputMode>(index);
    if (!task_.outputPath.isEmpty()) {
        task_.outputPath = videoExportPrimaryOutputPath(task_.outputPath, task_.outputMode);
    }
    emit outputChanged();
    savePreferences();
}

namespace {

using miacode::video_export::VideoQualityLevel;
using miacode::video_export::VideoRateMode;

// Page order of the rate-control and quality combos.
constexpr VideoRateMode kRateModeOrder[] = {VideoRateMode::Quality, VideoRateMode::FileSize, VideoRateMode::Bitrate};
constexpr VideoQualityLevel kQualityLevelOrder[] = {
    VideoQualityLevel::High, VideoQualityLevel::Standard, VideoQualityLevel::Small};

}  // namespace

QVariantList ExportSession::rateModeOptions() const
{
    return {QVariantMap{{"value", 0}, {"label", qtTrId("dialog.video_export.rate_mode.quality")}},
            QVariantMap{{"value", 1}, {"label", qtTrId("dialog.video_export.rate_mode.file_size")}},
            QVariantMap{{"value", 2}, {"label", qtTrId("dialog.video_export.rate_mode.bitrate")}}};
}

QVariantList ExportSession::qualityLevelOptions() const
{
    return {QVariantMap{{"value", 0}, {"label", qtTrId("dialog.video_export.quality_level.high")}},
            QVariantMap{{"value", 1}, {"label", qtTrId("dialog.video_export.quality_level.standard")}},
            QVariantMap{{"value", 2}, {"label", qtTrId("dialog.video_export.quality_level.small")}}};
}

int ExportSession::rateModeIndex() const
{
    return static_cast<int>(std::find(std::begin(kRateModeOrder), std::end(kRateModeOrder), task_.encoding.mode)
                            - std::begin(kRateModeOrder));
}

int ExportSession::qualityLevelIndex() const
{
    return static_cast<int>(
        std::find(std::begin(kQualityLevelOrder), std::end(kQualityLevelOrder), task_.encoding.quality)
        - std::begin(kQualityLevelOrder));
}

double ExportSession::estimatedSizeBytes() const
{
    const VideoExportTask task = requestedTaskSettings();
    const miacode::video_export::VideoExportTiming timing = miacode::video_export::videoExportTiming(task);
    miacode::video_export::VideoRateInputs inputs;
    inputs.width = task.outputWidth;
    inputs.height = task.outputHeight;
    inputs.fps = task.fps;
    inputs.videoFrameCount = timing.outputFrameCount;
    inputs.seconds = timing.outputSeconds;
    inputs.audioKbps = miacode::video_export::normalizedVideoExportAudioBitrateKbps(task.audioBitrateKbps);
    inputs.encoder = miacode::video_export::plannedVideoEncoder();
    const miacode::video_export::VideoRatePlan plan = miacode::video_export::planVideoRate(task.encoding, inputs);
    return plan.valid ? static_cast<double>(plan.estimatedBytes) : -1.0;
}

int ExportSession::introBackgroundModeIndex() const
{
    return task_.intro.backgroundMode.compare(QStringLiteral("custom"), Qt::CaseInsensitive) == 0 ? 1 : 0;
}

int ExportSession::introModeIndex() const
{
    if (isAutoIntroBannerMode(task_.intro.mode)) {
        return 0;
    }
    if (task_.intro.mode.compare(QStringLiteral("Standard"), Qt::CaseInsensitive) == 0) {
        return 2;
    }
    return 1;
}

bool ExportSession::introLevelTextRender() const
{
    return task_.intro.lvRenderMode.compare(QStringLiteral("text"), Qt::CaseInsensitive) == 0;
}

QVariantList ExportSession::introSoundOptions() const
{
    QVariantList list;
    list.append(QVariantMap{
        {QStringLiteral("label"),
         qtTrId("dialog.render_settings.music.default_intro_sound")},
        {QStringLiteral("fileName"), QString()},
    });
    const QString musicDirectory = miacode::preview_sfx::assetMusicDirectory();
    if (musicDirectory.isEmpty()) {
        return list;
    }
    const QFileInfoList entries = QDir(musicDirectory).entryInfoList(
        miacode::preview_sfx::supportedIntroSoundFileExtensions(),
        QDir::Files,
        QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo& entry : entries) {
        list.append(QVariantMap{
            {QStringLiteral("label"), entry.fileName()},
            {QStringLiteral("fileName"), entry.fileName()},
        });
    }
    return list;
}

int ExportSession::introSoundIndex() const
{
    const QString selected = miacode::preview_sfx::normalizeIntroSoundFileName(
        task_.introSoundFileName);
    const QVariantList options = introSoundOptions();
    for (int i = 0; i < options.size(); ++i) {
        if (options.at(i).toMap().value(QStringLiteral("fileName")).toString() == selected) {
            return i;
        }
    }
    return 0;
}

QVariantList ExportSession::fontLibraryOptions() const
{
    const QString defaultLabel = qtTrId("card_font.default");
    if (fontLibraryOptionsCacheValid_
        && fontLibraryOptionsCacheDefaultLabel_ == defaultLabel) {
        return fontLibraryOptionsCache_;
    }
    QVariantList list;
    const QVector<miacode::video_export::FontLibraryEntry> entries =
        miacode::video_export::fontLibraryEntries(miacode::app_preferences::fontLibraryDirectory(), true, defaultLabel);
    for (const miacode::video_export::FontLibraryEntry& entry : entries) {
        list.append(QVariantMap{
            {QStringLiteral("label"), entry.label},
            {QStringLiteral("path"), entry.path},
            {QStringLiteral("family"), entry.family},
        });
    }
    fontLibraryOptionsCache_ = list;
    fontLibraryOptionsCacheDefaultLabel_ = defaultLabel;
    fontLibraryOptionsCacheValid_ = true;
    return fontLibraryOptionsCache_;
}

QString ExportSession::introSoundLabel() const
{
    return qtTrId("dialog.render_settings.music.intro_sound");
}

QString ExportSession::introSoundImportLabel() const
{
    return qtTrId("dialog.render_settings.video.skin.import");
}

double ExportSession::exportEndSeconds() const
{
    return task_.exportStartSeconds + qMax(0.0, task_.contentDurationSeconds);
}

double ExportSession::minimumExportRangeSeconds() const
{
    return minimumExportRangeSecondsForChart(chartDurationSeconds_);
}

IntroBannerSpec ExportSession::previewIntroSpec() const
{
    IntroBannerSpec spec = task_.intro;
    spec.enabled = task_.intro.enabled && task_.fullRangeExport;
    // The export rebuilds the card from the chart; the preview resolves "auto"
    // the same way instead of handing it to the card, which reads it as DX.
    spec.mode = resolvedIntroBannerMode(task_.intro.mode, detectedIntroMode_);
    return spec;
}

QVariantList ExportSession::batchDifficultyChecks() const
{
    QVariantList list;
    for (const QVariant& option : difficultyOptions({1, 2, 3, 4, 5, 6, 7})) {
        QVariantMap row = option.toMap();
        const int id = row.value(QStringLiteral("id")).toInt();
        row.insert(QStringLiteral("checked"), batchSelectedDifficultyIds_.contains(id));
        list.append(row);
    }
    return list;
}

void ExportSession::setUnavailableReason(const QString& reason)
{
    if (unavailableReason_ == reason) {
        return;
    }
    unavailableReason_ = reason;
    emit unavailableReasonChanged();
    emit rangePlaybackStateChanged();
}

bool ExportSession::difficultyExists(int difficultyId) const
{
    // documentDifficultyIds() lists exactly the difficulties the document
    // holds, so membership is the same question as difficulty(id) != nullptr.
    return engine() != nullptr
        && SimaiDocument::isDifficultyId(difficultyId)
        && engine()->difficultyIds().contains(difficultyId);
}

int ExportSession::resolveDefaultDifficultyId(int previousActiveDifficultyId) const
{
    if (difficultyExists(previousActiveDifficultyId)) {
        return previousActiveDifficultyId;
    }
    if (difficultyExists(selectedDifficultyId_)) {
        return selectedDifficultyId_;
    }
    if (engine() != nullptr && difficultyExists(engine()->lastOpenedDifficultyId())) {
        return engine()->lastOpenedDifficultyId();
    }
    if (engine() != nullptr) {
        const QVector<int> ids = engine()->difficultyIds();
        if (!ids.isEmpty()) {
            return ids.constFirst();
        }
    }
    return 0;
}

void ExportSession::enter(int previousActiveDifficultyId)
{
    if (!pageSessionActive_) {
        pageSessionActive_ = true;
        emit pageSessionActiveChanged();
        emit rangePlaybackStateChanged();
    }
    const int nextDifficultyId = resolveDefaultDifficultyId(previousActiveDifficultyId);
    if (selectedDifficultyId_ != nextDifficultyId) {
        selectedDifficultyId_ = nextDifficultyId;
        emit selectedDifficultyIdChanged();
    }
    rebuildDifficultyList();
    // PageHost marks the QML page after this returns. Seed waits one tick.
    const quint64 generation = ++pagePrepareGeneration_;
    QTimer::singleShot(0, this, [this, generation]() {
        if (!pageSessionActive_ || generation != pagePrepareGeneration_) {
            return;
        }
        seedFromDifficulty(selectedDifficultyId_);
        syncAudition();
    });
    // Re-scan once per real page entry so imports made by another QML surface
    // are visible, while repeated property reads during this entry share the
    // materialized option list.
    fontLibraryOptionsCacheValid_ = false;
    emit fontLibraryChanged();
}

void ExportSession::leave()
{
    // Idle no-op: must not tear down the v1 Widgets export audition/session.
    if (!pageSessionActive_) {
        return;
    }
    setRangePlaybackEnabled(false);
    ++pagePrepareGeneration_;
    if (!hasSeededTask_) {
        clearPendingSelectionRangeExport();
    }
    pageSessionActive_ = false;
    emit pageSessionActiveChanged();
    emit rangePlaybackStateChanged();
    savePreferences();
    introPvSegmentDragging_ = false;
    emit introPreviewStateChanged();
    hasSeededTask_ = false;
    stopAudition();
    setUnavailableReason(QString());
}

void ExportSession::replaceDocument(int preferredDifficultyId)
{
    setRangePlaybackEnabled(false);
    ++pagePrepareGeneration_;
    stopAudition();
    clearPendingSelectionRangeExport();

    const bool retainUserSettings = hasSeededTask_;
    const VideoExportTask previousTask = task_;
    task_ = VideoExportTask();
    if (retainUserSettings) {
        miacode::video_export::copyVideoExportUserSettings(previousTask, &task_);
    }
    chartDurationSeconds_ = 0.0;
    task_.exportStartSeconds = 0.0;
    task_.contentDurationSeconds = 0.0;
    task_.fullRangeExport = true;

    batchSelectedDifficultyIds_.clear();
    int nextDifficultyId = difficultyExists(preferredDifficultyId)
        ? preferredDifficultyId : 0;
    if (nextDifficultyId == 0 && engine() != nullptr) {
        const QList<int> ids = engine()->difficultyIds();
        if (!ids.isEmpty()) {
            nextDifficultyId = ids.constFirst();
        }
    }
    if (selectedDifficultyId_ != nextDifficultyId) {
        selectedDifficultyId_ = nextDifficultyId;
        emit selectedDifficultyIdChanged();
    }
    rebuildDifficultyList();

    if (pageSessionActive_) {
        seedFromDifficulty(selectedDifficultyId_);
        syncAudition();
    }
    if (!difficultyExists(selectedDifficultyId_)) {
        emit outputChanged();
        emit videoChanged();
        emit introChanged();
        emit rangeChanged();
        emit batchChanged();
    }
}

void ExportSession::selectDifficulty(int difficultyId)
{
    const int next = difficultyExists(difficultyId) ? difficultyId : 0;
    if (selectedDifficultyId_ == next) {
        return;
    }
    selectedDifficultyId_ = next;
    emit selectedDifficultyIdChanged();
    if (!pageSessionActive_) {
        return;
    }
    ++pagePrepareGeneration_;
    seedFromDifficulty(selectedDifficultyId_);
    syncAudition();
}

void ExportSession::setActiveTab(const QString& tabId)
{
    const QString next = tabId == QLatin1String("batch")
        ? QStringLiteral("batch")
        : QStringLiteral("export");
    if (activeTab_ == next) {
        return;
    }
    activeTab_ = next;
    if (!rangePreviewAvailable()) {
        setRangePlaybackEnabled(false);
    }
    if (activeTab_ == QLatin1String("batch")) {
        setSettingsTab(QStringLiteral("batch"));
    } else if (settingsTab_ == QLatin1String("batch")) {
        setSettingsTab(QStringLiteral("output"));
    }
    emit activeTabChanged();
    emit rangePlaybackStateChanged();
    if (pageSessionActive_) {
        syncAudition();
    }
}

void ExportSession::setSettingsTab(const QString& tabId)
{
    const QString next = tabId == QLatin1String("range")
        || (tabId == QLatin1String("batch") && activeTab_ != QLatin1String("batch"))
        ? QStringLiteral("output")
        : tabId;
    if (settingsTab_ == next) {
        return;
    }
    settingsTab_ = next;
    emit settingsTabChanged();
}

void ExportSession::rebuildDifficultyList()
{
    const QVariantList next = engine() != nullptr
        ? difficultyOptions(engine()->difficultyIds()) : QVariantList();
    if (difficulties_ != next) {
        difficulties_ = next;
        emit difficultiesChanged();
    }
}

void ExportSession::refreshFromDocument()
{
    if (!difficultyExists(selectedDifficultyId_)) {
        selectedDifficultyId_ = resolveDefaultDifficultyId(0);
        emit selectedDifficultyIdChanged();
    }
    rebuildDifficultyList();
    if (pageSessionActive_) {
        ++pagePrepareGeneration_;
        seedFromDifficulty(selectedDifficultyId_);
        syncAudition();
    }
    emit batchChanged();
}

void ExportSession::applyPreferences()
{
    const QJsonObject settings = miacode::app_preferences::loadDialogPreferences();
    // Keep the established first-run defaults shared with the Widgets dialog.
    task_.clockCountEnabled = false;
    task_.intro.mode = QStringLiteral("auto");
    task_.intro.lvRenderMode = QStringLiteral("atlas");
    miacode::video_export::applyVideoExportPreferences(settings, &task_);
    const int savedWidth = task_.outputWidth;
    const int savedHeight = task_.outputHeight;
    resolutionIndex_ = 3;
    for (int i = 0; i < static_cast<int>(std::size(kResolutionPresets)); ++i) {
        if (kResolutionPresets[i].width == savedWidth && kResolutionPresets[i].height == savedHeight) {
            resolutionIndex_ = i;
            break;
        }
    }
    task_.outputWidth = kResolutionPresets[resolutionIndex_].width;
    task_.outputHeight = kResolutionPresets[resolutionIndex_].height;
}

void ExportSession::savePreferences() const
{
    QJsonObject settings = miacode::app_preferences::loadDialogPreferences();
    miacode::video_export::appendVideoExportPreferences(&settings, task_);
    miacode::app_preferences::saveDialogPreferences(settings);
}

void ExportSession::seedFromDifficulty(int difficultyId)
{
    if (!difficultyExists(difficultyId)) {
        VideoExportTask emptyTask;
        if (hasSeededTask_) {
            miacode::video_export::copyVideoExportUserSettings(task_, &emptyTask);
        }
        task_ = std::move(emptyTask);
        detectedIntroMode_ = QStringLiteral("Standard");
        chartDurationSeconds_ = 0.0;
        if (!hasSeededTask_) {
            applyPreferences();
            hasSeededTask_ = true;
        }
        setUnavailableReason(qtTrId("export_page.no_difficulty_is_available_to"));
        setRangePlaybackEnabled(false);
        clearPendingSelectionRangeExport();
        emit outputChanged();
        emit videoChanged();
        emit introChanged();
        emit rangeChanged();
        return;
    }
    setUnavailableReason(QString());
    VideoExportTask seededTask = engine()->buildSeedTask(difficultyId);
    detectedIntroMode_ = normalizedIntroBannerMode(seededTask.intro.mode);
    if (hasSeededTask_) {
        miacode::video_export::copyVideoExportUserSettings(task_, &seededTask);
    }
    task_ = std::move(seededTask);
    chartDurationSeconds_ = qMax(0.0, task_.contentDurationSeconds);
    if (!hasSeededTask_) {
        applyPreferences();
        hasSeededTask_ = true;
    }
    task_.exportStartSeconds = 0.0;
    task_.contentDurationSeconds = chartDurationSeconds_;
    task_.fullRangeExport = true;
    if (batchSelectedDifficultyIds_.isEmpty()) {
        batchSelectedDifficultyIds_.append(difficultyId);
    }
    if (batchOutputDirectory_.isEmpty()) {
        // A stored empty value is a cleared choice and must not revive v1's.
        const QJsonObject preferences = miacode::app_preferences::loadDialogPreferences();
        batchOutputDirectory_ = preferences.contains(QLatin1String(kBatchOutputDirectoryKey))
            ? miacode::file_dialog_preferences::validDirectory(
                  preferences.value(QLatin1String(kBatchOutputDirectoryKey)).toString())
            : legacyBatchExportDirectory("last_output_directory");
    }
    adoptPreviewRenderSettings();
    emit outputChanged();
    emit videoChanged();
    emit introChanged();
    emit rangeChanged();
    emit batchChanged();
}

void ExportSession::syncAudition()
{
    if (engine() == nullptr || !pageSessionActive_) {
        return;
    }
    if (!difficultyExists(selectedDifficultyId_)) {
        engine()->clearAudition();
        return;
    }
    const bool applySelectionRange = hasPendingSelectionRangeExport_;
    if (applySelectionRange) {
        setExportRangeSeconds(pendingRangeStartSeconds_, pendingRangeEndSeconds_);
    }
    engine()->startAudition(selectedDifficultyId_, task_);
    if (applySelectionRange) {
        hasPendingSelectionRangeExport_ = false;
        setSettingsTab(QStringLiteral("output"));
    }
    if (applySelectionRange && !rangePlaybackEnabled_) {
        setRangePlaybackEnabled(true);
    } else if (rangePlaybackEnabled_) {
        // 安装试听场景会重设播放头边界；目标场景就绪后同步区间。
        emit playbackRangeRequested(true, exportStartSeconds(), exportEndSeconds());
    }
}

void ExportSession::applyLivePreviewSettings()
{
    if (engine() == nullptr) {
        return;
    }
    adoptPreviewRenderSettings();
    VideoExportTask liveTask = task_;
    applyOwnerLiveFields(&liveTask);
    {
        const QScopedValueRollback pushing(pushingSharedSettings_, true);
        engine()->applySharedTaskSettings(liveTask);
    }
}

void ExportSession::adoptPreviewRenderSettings()
{
    // The task is seeded from this live state on page entry. Left alone, the page would
    // keep that entry-time copy and write it back over the newer value on its next edit
    // or export.
    if (preview() == nullptr) {
        return;
    }
    const QVariantMap values = preview()->renderSettings();
    const auto fromPercent = [&values](const char* key, double fallback) {
        const QVariant value = values.value(QLatin1String(key));
        return value.isValid() ? value.toDouble() / 100.0 : fallback;
    };
    task_.backgroundBrightnessOuter = fromPercent("brightnessOuter", task_.backgroundBrightnessOuter);
    task_.backgroundBrightnessInner = fromPercent("brightnessInner", task_.backgroundBrightnessInner);
    task_.layoutSquareScale = fromPercent("layoutSquareScale", task_.layoutSquareScale);
    task_.backgroundScaleMode = static_cast<PreviewBackgroundScaleMode>(
        values.value(QStringLiteral("scaleMode"), static_cast<int>(task_.backgroundScaleMode)).toInt());
    task_.fireworkBrightness = miacode::preview_video::normalizedFireworkBrightness(fromPercent("fireworkBrightness", task_.fireworkBrightness));
    task_.fireworkStyle = static_cast<PreviewFireworkStyle>(values.value(QStringLiteral("fireworkStyle"), static_cast<int>(task_.fireworkStyle)).toInt());
    task_.smoothBrightness = values.value(QStringLiteral("smoothBrightness"), task_.smoothBrightness).toBool();
    task_.showTimestamp = values.value(QStringLiteral("showTimestamp"), task_.showTimestamp).toBool();
    task_.tapFlowSpeed = values.value(QStringLiteral("tapFlowSpeed"), task_.tapFlowSpeed).toDouble();
    task_.touchFlowSpeed = values.value(QStringLiteral("touchFlowSpeed"), task_.touchFlowSpeed).toDouble();
}

void ExportSession::stopAudition()
{
    if (engine() == nullptr) {
        return;
    }
    engine()->stopAudition();
}

void ExportSession::applyOwnerLiveFields(VideoExportTask* task) const
{
    if (task == nullptr || preview() == nullptr) {
        return;
    }
    task->outlineVariant = appearance_->outlineVariant();
    task->slideEarlierSecondAndTextOnTop = appearance_->slideEarlierSecondAndTextOnTop();
    task->tapJudgeTextDistance = appearance_->tapJudgeTextDistance();
    task->judgeEffectStyle = appearance_->judgeEffectStyle();
    task->centerDisplayMode = appearance_->centerDisplayMode();
    task->muriRenderOptions = engine()->muriRenderOptions();
}

VideoExportTask ExportSession::requestedTaskSettings() const
{
    VideoExportTask task = task_;
    task.outputWidth = kResolutionPresets[qBound(0, resolutionIndex_, static_cast<int>(std::size(kResolutionPresets)) - 1)].width;
    task.outputHeight = kResolutionPresets[qBound(0, resolutionIndex_, static_cast<int>(std::size(kResolutionPresets)) - 1)].height;
    task.fullRangeExport = miacode::video_export::isFullRangeVideoExport(task.exportStartSeconds);
    task.intro.enabled = task.intro.enabled && task.fullRangeExport;
    return task;
}

VideoExportTask ExportSession::buildRequestedTask() const
{
    VideoExportTask task = requestedTaskSettings();
    applyOwnerLiveFields(&task);
    return task;
}

void ExportSession::startExport()
{
    if (engine() == nullptr) {
        return;
    }
    ++pagePrepareGeneration_;
    if (!hasSeededTask_) {
        seedFromDifficulty(selectedDifficultyId_);
        syncAudition();
    }
    adoptPreviewRenderSettings();
    if (activeTab_ == QLatin1String("batch")) {
        savePreferences();
        batchCancellationRequested_ = false;
        batchExportRunning_ = true;
        exportRunning_ = true;
        emit exportRunningChanged();
        miacode::ExportEngine::BatchResult result;
        miacode::ExportEngine::BatchCallbacks callbacks;
        // Batch runs synchronously on the UI thread, so it reports onto the same
        // shell overlay every other job uses. Before this the callback pumped
        // events and threw the percentage away, leaving batch with no progress
        // at all.
        miacode::JobProgressService* const jobProgress =
            jobProgress_;
        const miacode::LocalizedText batchJobTitle = miacode::localizedText("action.batch_export");
        quint64 batchJobToken = 0;
        if (jobProgress != nullptr) {
            batchJobToken = jobProgress->begin(
                batchJobTitle,
                miacode::localizedText("export.preparing_package"),
                /*cancellable=*/true,
                miacode::JobProgressService::TaskType::ChartExport);
        }
        callbacks.progressChanged = [jobProgress, batchJobToken](int percent, const QString& label) {
            if (jobProgress != nullptr && jobProgress->token() == batchJobToken) {
                jobProgress->report(percent, label);
            }
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        };
        callbacks.cancellationRequested = [this, jobProgress, batchJobToken]() {
            const bool shellCancelled = jobProgress != nullptr
                && jobProgress->token() == batchJobToken
                && jobProgress->cancelRequested();
            return batchCancellationRequested_ || shellCancelled;
        };
        VideoExportTask batchTask = buildRequestedTask();
        // Batch is always full-range, so keep the user's intro preference even
        // if the single-export range currently starts after chart zero.
        batchTask.intro.enabled = task_.intro.enabled;
        QString error;
        bool launched = false;
        {
            const QScopedValueRollback pushing(pushingSharedSettings_, true);
            launched = engine()->launchBatchExport(
                batchTask,
                chartDirectories_,
                batchSelectedDifficultyIds_,
                batchOutputDirectory_,
                &result,
                callbacks,
                &error);
        }
        if (jobProgress != nullptr && jobProgress->token() == batchJobToken) {
            jobProgress->end();
        }
        batchExportRunning_ = false;
        exportRunning_ = false;
        emit exportRunningChanged();
        if (launched && result.successCount > 0) {
            resetIntroSelection();
        }
        const miacode::LocalizedText batchTitle = miacode::localizedText("action.batch_export");
        if (!launched) {
            uiRequests_->postNotice(
                miacode::NoticeSeverity::Error,
                batchTitle,
                error.isEmpty()
                    ? miacode::localizedText("dialog.video_export.error.failed")
                    : miacode::LocalizedText(error));
            return;
        }
        if (result.canceled) {
            uiRequests_->postNotice(
                miacode::NoticeSeverity::Information,
                batchTitle,
                miacode::localizedText("dialog.batch_export.message.canceled"));
            return;
        }
        const auto shortenDetails = [](QString details) {
            return details.size() > 3000 ? details.left(3000) + QStringLiteral("\n...") : details;
        };
        const QString successDetails = shortenDetails(result.exportedFiles.join(QLatin1Char('\n')));
        if (result.failedCharts.isEmpty()) {
            uiRequests_->postNotice(
                miacode::NoticeSeverity::Information,
                batchTitle,
                miacode::localizedText("dialog.batch_export.message.success").arg(result.successCount),
                successDetails);
            return;
        }
        uiRequests_->postNotice(
            miacode::NoticeSeverity::Warning,
            batchTitle,
            miacode::localizedText("dialog.batch_export.message.partial_failed")
                .arg(result.successCount).arg(result.failedCharts.size()),
            (successDetails.isEmpty() ? miacode::LocalizedText()
                : miacode::localizedText("dialog.batch_export.message.output_files")
                    + QStringLiteral("\n") + successDetails + QStringLiteral("\n\n"))
                + shortenDetails(result.failedCharts.join(QLatin1Char('\n'))));
        return;
    }

    if (!difficultyExists(selectedDifficultyId_)) {
        return;
    }
    savePreferences();
    QString error;
    exportRunning_ = true;
    emit exportRunningChanged();
    bool launched = false;
    {
        const QScopedValueRollback pushing(pushingSharedSettings_, true);
        launched = engine()->launchVideoExport(buildRequestedTask(), selectedDifficultyId_, &error);
    }
    if (!launched) {
        exportRunning_ = false;
        emit exportRunningChanged();
        uiRequests_->postNotice(
            miacode::NoticeSeverity::Error,
            miacode::localizedText("dialog.video_export.title"),
            error.isEmpty()
                ? miacode::localizedText("dialog.video_export.error.launch_failed")
                : miacode::LocalizedText(error));
        return;
    }
}

void ExportSession::cancelExport()
{
    if (engine() == nullptr) {
        return;
    }
    if (batchExportRunning_) {
        batchCancellationRequested_ = true;
        return;
    }
    engine()->cancelVideoExport();
}

void ExportSession::browseOutputPath()
{
    miacode::FileRequest request;
    request.title = miacode::localizedText("dialog.video_export.title");
    const QString directory = task_.chartPath.isEmpty() ? QDir::currentPath() : QFileInfo(task_.chartPath).absolutePath();
    request.startPath = QDir(directory).absoluteFilePath(task_.outputPath.isEmpty()
        ? (task_.outputMode == VideoExportOutputMode::Wav ? QStringLiteral("out.wav") : QStringLiteral("out.mp4"))
        : task_.outputPath);
    request.nameFilters = {miacode::localizedText(
        task_.outputMode == VideoExportOutputMode::Wav ? "file_filter.wav" : "file_filter.mp4")};
    request.saveMode = true;
    uiRequests_->requestFile(request, [this](const QString& path) {
        if (!path.isEmpty()) {
            setOutputPath(path);
        }
    });
}

void ExportSession::browseIntroBackground()
{
    miacode::FileRequest request;
    request.title = miacode::localizedText("dialog.video_export.choose_intro_background");
    request.startPath = task_.intro.customBackgroundPath;
    request.nameFilters = {miacode::localizedText("file_filter.image")};
    uiRequests_->requestFile(request, [this](const QString& path) {
        if (!path.isEmpty()) {
            setIntroCustomBackgroundPath(path);
        }
    });
}

void ExportSession::importIntroSound()
{
    miacode::FileRequest request;
    request.title = miacode::localizedText("dialog.render_settings.music.intro_sound");
    request.startPath = miacode::file_dialog_preferences::recentDirectory(miacode::file_dialog_preferences::Purpose::IntroSound);
    request.nameFilters = {miacode::localizedText("file_filter.audio")};
    uiRequests_->requestFile(request, [this](const QString& path) {
        miacode::file_dialog_preferences::rememberFile(miacode::file_dialog_preferences::Purpose::IntroSound, path);
        applyIntroSoundImport(path);
    });
}

void ExportSession::importIntroFont()
{
    miacode::FileRequest request;
    request.title = miacode::localizedText("card_font.import");
    request.startPath = miacode::file_dialog_preferences::recentDirectory(miacode::file_dialog_preferences::Purpose::IntroFont);
    request.nameFilters = {miacode::localizedText("file_filter.font")};
    uiRequests_->requestFile(request, [this](const QString& path) {
        miacode::file_dialog_preferences::rememberFile(miacode::file_dialog_preferences::Purpose::IntroFont, path);
        applyFontImport(path);
    });
}

void ExportSession::applyFontImport(const QString& selectedPath)
{
    if (selectedPath.isEmpty()) {
        return;
    }
    const miacode::video_export::FontImportResult result =
        miacode::video_export::importFontFileIntoLibrary(selectedPath, miacode::app_preferences::fontLibraryDirectory());
    if (result.path.isEmpty()) {
        const miacode::LocalizedText text = result.failure == miacode::video_export::FontImportFailure::CopyFailed
            ? miacode::localizedText("card_font.copy_failed")
            : miacode::localizedText("card_font.invalid_font");
        uiRequests_->postNotice(miacode::NoticeSeverity::Warning,
                                miacode::localizedText("card_font.import"), text);
        return;
    }

    fontLibraryOptionsCacheValid_ = false;
    emit fontLibraryChanged();
    // Match the established card-font picker: an imported font becomes the
    // title/display choice while the body selection remains independent.
    setIntroFontDisplayPath(result.path);
}

void ExportSession::applyIntroSoundImport(const QString& selectedPath)
{
    if (selectedPath.isEmpty()) {
        return;
    }

    const QString musicDirectory = miacode::preview_sfx::assetMusicDirectory();
    if (musicDirectory.isEmpty() || !QDir().mkpath(musicDirectory)) {
        return;
    }

    const QFileInfo sourceInfo(selectedPath);
    QString importedName = sourceInfo.fileName();
    QString importedPath = QDir(musicDirectory).filePath(importedName);
    if (QFileInfo(selectedPath).canonicalFilePath() != QFileInfo(importedPath).canonicalFilePath()) {
        int suffix = 2;
        while (QFileInfo::exists(importedPath)) {
            importedName = QStringLiteral("%1_%2.%3")
                .arg(sourceInfo.completeBaseName())
                .arg(suffix++)
                .arg(sourceInfo.suffix());
            importedPath = QDir(musicDirectory).filePath(importedName);
        }
        if (!QFile::copy(selectedPath, importedPath)) {
            return;
        }
    }

    emit introSoundOptionsChanged();
    setIntroSoundFileName(importedName);
}

void ExportSession::browseBatchOutputDirectory()
{
    miacode::FileRequest request;
    request.title = miacode::localizedText("dialog.batch_export.select_folder");
    request.startPath = miacode::file_dialog_preferences::validDirectory(batchOutputDirectory_);
    if (request.startPath.isEmpty()) {
        request.startPath = QDir::homePath();
    }
    request.selectFolder = true;
    uiRequests_->requestFile(request, [this](const QString& path) {
        if (!path.isEmpty()) {
            setBatchOutputDirectory(path);
        }
    });
}

void ExportSession::addChartDirectories()
{
    miacode::FileRequest request;
    request.title = miacode::localizedText("dialog.batch_export.select_charts");
    request.selectFolder = true;
    const QJsonObject preferences = miacode::app_preferences::loadDialogPreferences();
    request.startPath = preferences.value(QStringLiteral("last_batch_chart_directory")).toString();
    if (request.startPath.isEmpty()) {
        const QString legacy = legacyBatchExportDirectory("last_chart_directory");
        if (!legacy.isEmpty()) request.startPath = QFileInfo(legacy).absolutePath();
    }
    if (!QDir(request.startPath).exists() || request.startPath.isEmpty()) {
        request.startPath = task_.chartPath.isEmpty() ? QDir::homePath()
            : QFileInfo(QFileInfo(task_.chartPath).absolutePath()).absolutePath();
    }
    uiRequests_->requestFiles(request, [self = QPointer<ExportSession>(this)](const QStringList& paths) {
        if (!self || paths.isEmpty()) return;
        for (const QString& path : paths) self->addChartDirectory(path);
        // Open the containing directory next time so sibling charts can be
        // added without repeatedly navigating back out of a chart folder.
        QJsonObject preferences = miacode::app_preferences::loadDialogPreferences();
        preferences.insert(QStringLiteral("last_batch_chart_directory"), QFileInfo(paths.last()).absolutePath());
        miacode::app_preferences::saveDialogPreferences(preferences);
    });
}

void ExportSession::addChartDirectory(const QString& path)
{
    if (path.isEmpty() || !QFileInfo(path).isDir()) {
        return;
    }
    const QString normalized = QFileInfo(path).canonicalFilePath();
    if (chartDirectories_.contains(normalized)) return;
    chartDirectories_.append(normalized);
    emit batchChanged();
}

void ExportSession::removeChartDirectory(int index)
{
    if (index < 0 || index >= chartDirectories_.size()) {
        return;
    }
    chartDirectories_.removeAt(index);
    emit batchChanged();
}

void ExportSession::clearChartDirectories()
{
    if (chartDirectories_.isEmpty()) {
        return;
    }
    chartDirectories_.clear();
    emit batchChanged();
}

void ExportSession::setBatchDifficultyChecked(int difficultyId, bool checked)
{
    if (!SimaiDocument::isDifficultyId(difficultyId)) {
        return;
    }
    if (checked) {
        if (!batchSelectedDifficultyIds_.contains(difficultyId)) {
            batchSelectedDifficultyIds_.append(difficultyId);
            emit batchChanged();
        }
    } else {
        if (batchSelectedDifficultyIds_.removeAll(difficultyId) > 0) {
            emit batchChanged();
        }
    }
}

void ExportSession::setExportStartToCurrentPreview()
{
    if (engine() == nullptr) {
        return;
    }
    setExportStartSeconds(engine()->currentAudioClockSecond());
}

void ExportSession::setExportEndToCurrentPreview()
{
    if (engine() == nullptr) {
        return;
    }
    setExportEndSeconds(engine()->currentAudioClockSecond());
}

void ExportSession::setExportRangeSeconds(double start, double end)
{
    if (!qIsFinite(start) || !qIsFinite(end)) {
        return;
    }
    const double minimumDuration = minimumExportRangeSeconds();
    const double boundedStart = qBound(0.0, start, qMax(0.0, chartDurationSeconds_ - minimumDuration));
    const double boundedEnd = qBound(boundedStart + minimumDuration, end, chartDurationSeconds_);
    if (boundedStart == task_.exportStartSeconds && boundedEnd == exportEndSeconds()) {
        return;
    }
    const bool previousFullRangeExport = task_.fullRangeExport;
    task_.exportStartSeconds = boundedStart;
    task_.contentDurationSeconds = qMax(0.0, boundedEnd - boundedStart);
    task_.fullRangeExport = miacode::video_export::isFullRangeVideoExport(task_.exportStartSeconds);
    if (task_.intro.enabled && previousFullRangeExport != task_.fullRangeExport && engine() != nullptr) {
        engine()->refreshIntroState();
    }
    emit rangeChanged();
    emit introChanged();
}

void ExportSession::requestSelectionRangeExport(double startSecond, double endSecond)
{
    if (!qIsFinite(startSecond) || !qIsFinite(endSecond) || endSecond <= startSecond) {
        return;
    }
    hasPendingSelectionRangeExport_ = true;
    pendingRangeStartSeconds_ = startSecond;
    pendingRangeEndSeconds_ = endSecond;
}

void ExportSession::clearPendingSelectionRangeExport()
{
    hasPendingSelectionRangeExport_ = false;
}

QString ExportSession::setExportStartText(const QString& text)
{
    double seconds = 0.0;
    const QString normalized = miacode::video_export::sanitizeVideoExportTimestamp(text);
    bool parsed = false;
    if (normalized.contains(QLatin1Char(':'))) {
        parsed = miacode::video_export::parseVideoExportTimestamp(normalized, &seconds);
    } else {
        seconds = normalized.toDouble(&parsed);
    }
    if (parsed) {
        setExportStartSeconds(seconds);
    }
    return QString::number(task_.exportStartSeconds, 'f', 3);
}

QString ExportSession::setIntroPvStartText(const QString& text)
{
    double seconds = 0.0;
    const QString normalized = miacode::video_export::sanitizeVideoExportTimestamp(text);
    bool parsed = false;
    if (normalized.contains(QLatin1Char(':'))) {
        parsed = miacode::video_export::parseVideoExportTimestamp(normalized, &seconds);
    } else {
        seconds = normalized.toDouble(&parsed);
    }
    if (parsed) {
        setIntroPvStartSeconds(seconds);
    }
    return QString::number(task_.intro.pvPreviewStartSeconds, 'f', 3);
}

QString ExportSession::setExportEndText(const QString& text)
{
    double seconds = 0.0;
    const QString normalized = miacode::video_export::sanitizeVideoExportTimestamp(text);
    bool parsed = false;
    if (normalized.contains(QLatin1Char(':'))) {
        parsed = miacode::video_export::parseVideoExportTimestamp(normalized, &seconds);
    } else {
        seconds = normalized.toDouble(&parsed);
    }
    if (parsed) {
        setExportEndSeconds(seconds);
    }
    return QString::number(exportEndSeconds(), 'f', 3);
}

void ExportSession::setOutputPath(const QString& path)
{
    if (task_.outputPath == path) {
        return;
    }
    task_.outputPath = path.trimmed().isEmpty() ? QString() : videoExportPrimaryOutputPath(path, task_.outputMode);
    emit outputChanged();
}

void ExportSession::setResolutionIndex(int index)
{
    if (index < 0 || index >= static_cast<int>(std::size(kResolutionPresets)) || resolutionIndex_ == index) {
        return;
    }
    resolutionIndex_ = index;
    task_.outputWidth = kResolutionPresets[index].width;
    task_.outputHeight = kResolutionPresets[index].height;
    emit outputChanged();
    savePreferences();
    if (pageSessionActive_) {
        applyLivePreviewSettings();
    }
}

void ExportSession::setFps(int fps)
{
    if (task_.fps == fps) {
        return;
    }
    task_.fps = fps;
    emit outputChanged();
    savePreferences();
}

void ExportSession::setAudioBitrateKbps(int kbps)
{
    if (task_.audioBitrateKbps == kbps) {
        return;
    }
    task_.audioBitrateKbps = kbps;
    emit outputChanged();
    savePreferences();
}

void ExportSession::applyEncodingSettings(const miacode::video_export::VideoEncodingSettings& encoding)
{
    const miacode::video_export::VideoEncodingSettings normalized =
        miacode::video_export::normalizedVideoEncodingSettings(encoding);
    if (task_.encoding == normalized) {
        return;
    }
    task_.encoding = normalized;
    emit outputChanged();
    savePreferences();
}

void ExportSession::setRateModeIndex(int index)
{
    if (index < 0 || index >= static_cast<int>(std::size(kRateModeOrder))) {
        return;
    }
    miacode::video_export::VideoEncodingSettings encoding = task_.encoding;
    encoding.mode = kRateModeOrder[index];
    applyEncodingSettings(encoding);
}

void ExportSession::setQualityLevelIndex(int index)
{
    if (index < 0 || index >= static_cast<int>(std::size(kQualityLevelOrder))) {
        return;
    }
    miacode::video_export::VideoEncodingSettings encoding = task_.encoding;
    encoding.quality = kQualityLevelOrder[index];
    applyEncodingSettings(encoding);
}

void ExportSession::setSizeLimitMb(int megabytes)
{
    miacode::video_export::VideoEncodingSettings encoding = task_.encoding;
    encoding.sizeLimitMb = megabytes;
    applyEncodingSettings(encoding);
}

void ExportSession::setVideoBitrateMbps(double megabitsPerSecond)
{
    if (!std::isfinite(megabitsPerSecond)) {
        return;
    }
    miacode::video_export::VideoEncodingSettings encoding = task_.encoding;
    encoding.videoBitrateKbps = qRound(megabitsPerSecond * 1000.0);
    applyEncodingSettings(encoding);
}

void ExportSession::setShowObjectStatsHud(bool value)
{
    task_.showObjectStatsHud = value;
    emit videoChanged();
    applyLivePreviewSettings();
}

void ExportSession::setShowChartInfoHud(bool value)
{
    task_.showChartInfoHud = value;
    emit videoChanged();
    applyLivePreviewSettings();
}


void ExportSession::setClockCountEnabled(bool value)
{
    task_.clockCountEnabled = value;
    emit videoChanged();
    savePreferences();
}

void ExportSession::setIntroEnabled(bool value)
{
    task_.intro.enabled = value;
    emit introChanged();
    if (engine() != nullptr) {
        engine()->refreshIntroState();
    }
}

void ExportSession::setIntroBackgroundModeIndex(int index)
{
    task_.intro.backgroundMode = index == 1 ? QStringLiteral("custom") : QStringLiteral("jacket");
    emit introChanged();
    savePreferences();
    if (engine() != nullptr) {
        engine()->refreshIntroState();
    }
}

void ExportSession::setIntroCustomBackgroundPath(const QString& path)
{
    task_.intro.customBackgroundPath = path;
    emit introChanged();
    savePreferences();
    if (engine() != nullptr) {
        engine()->refreshIntroState();
    }
}

void ExportSession::setIntroBlurBackground(bool value)
{
    task_.intro.blurBackground = value;
    emit introChanged();
    savePreferences();
    if (engine() != nullptr) {
        engine()->refreshIntroState();
    }
}

void ExportSession::setIntroModeIndex(int index)
{
    if (index == 0) {
        task_.intro.mode = QStringLiteral("auto");
    } else if (index == 2) {
        task_.intro.mode = QStringLiteral("Standard");
    } else {
        task_.intro.mode = QStringLiteral("DX");
    }
    emit introChanged();
    savePreferences();
    if (engine() != nullptr) {
        engine()->refreshIntroState();
    }
}

void ExportSession::setIntroCardShadow(bool value)
{
    task_.intro.cardShadow = value;
    emit introChanged();
    savePreferences();
    if (engine() != nullptr) {
        engine()->refreshIntroState();
    }
}

void ExportSession::setIntroLevelTextRender(bool value)
{
    task_.intro.lvRenderMode = value ? QStringLiteral("text") : QStringLiteral("atlas");
    emit introChanged();
    savePreferences();
    if (engine() != nullptr) {
        engine()->refreshIntroState();
    }
}

void ExportSession::setIntroFontDisplayPath(const QString& path)
{
    if (task_.intro.fontDisplayPath == path) {
        return;
    }
    task_.intro.fontDisplayPath = path;
    emit introChanged();
    savePreferences();
    if (engine() != nullptr) {
        engine()->refreshIntroState();
    }
}

void ExportSession::setIntroFontBodyPath(const QString& path)
{
    if (task_.intro.fontBodyPath == path) {
        return;
    }
    task_.intro.fontBodyPath = path;
    emit introChanged();
    savePreferences();
    if (engine() != nullptr) {
        engine()->refreshIntroState();
    }
}

double ExportSession::introPvSegmentSeconds() const
{
    return miacode::intro::kPvPreviewHoldSeconds;
}

double ExportSession::introPvMusicSeconds() const
{
    return miacode::intro::kPvPreviewHoldSeconds + miacode::intro::kPvPreviewAudioTailSeconds;
}

bool ExportSession::introPvVideoAvailable() const
{
    return miacode::chart_assets::isVideoBackgroundPath(task_.backgroundMediaPath)
        && QFileInfo::exists(task_.backgroundMediaPath);
}

void ExportSession::setIntroPvPreview(bool value)
{
    if (task_.intro.pvPreview == value) {
        return;
    }
    task_.intro.pvPreview = value;
    emit introChanged();
    if (engine() != nullptr) {
        engine()->refreshIntroState();
    }
}

void ExportSession::setIntroPvStartSeconds(double seconds)
{
    if (!std::isfinite(seconds)) {
        return;
    }
    const double latestStart = qMax(0.0, chartDurationSeconds_ - miacode::intro::kPvPreviewHoldSeconds);
    seconds = qBound(0.0, seconds, latestStart);
    if (qAbs(seconds - task_.intro.pvPreviewStartSeconds) < 1e-6) {
        return;
    }
    task_.intro.pvPreviewStartSeconds = seconds;
    introAuditionFromHead_ = true;
    emit introChanged();
    if (engine() != nullptr) {
        engine()->refreshIntroState();
    }
}

void ExportSession::setIntroPvBrightness(double value)
{
    if (!std::isfinite(value)) {
        return;
    }
    value = qBound(0.0, value, 1.0);
    if (qAbs(value - task_.intro.pvPreviewBrightness) < 1e-6) {
        return;
    }
    task_.intro.pvPreviewBrightness = value;
    emit introChanged();
    savePreferences();
    if (engine() != nullptr) {
        engine()->refreshIntroState();
    }
}

bool ExportSession::introAuditionPlaying() const
{
    return engine() != nullptr && engine()->introAuditionPlaying();
}

void ExportSession::toggleIntroAudition()
{
    if (engine() == nullptr) {
        return;
    }
    engine()->setIntroAuditionPlaying(!engine()->introAuditionPlaying());
    emit introPreviewStateChanged();
}

void ExportSession::noteIntroPvSeek()
{
    introAuditionFromHead_ = false;
}

void ExportSession::setIntroPvSegmentDragging(bool dragging)
{
    if (introPvSegmentDragging_ == dragging) {
        return;
    }
    introPvSegmentDragging_ = dragging;
    emit introPreviewStateChanged();
    if (engine() != nullptr) {
        engine()->refreshIntroState();
    }
}

void ExportSession::setIntroAuditionLoop(bool loop)
{
    if (introAuditionLoop_ == loop) {
        return;
    }
    introAuditionLoop_ = loop;
    emit introPreviewStateChanged();
}

void ExportSession::resetIntroSelection()
{
    task_.intro.enabled = true;
    task_.intro.pvPreview = false;
    task_.intro.pvPreviewStartSeconds = 0.0;
    introPvSegmentDragging_ = false;
    introAuditionFromHead_ = true;
    emit introChanged();
    emit introPreviewStateChanged();
    if (engine() != nullptr) {
        engine()->refreshIntroState();
    }
}

void ExportSession::resetIntroFonts()
{
    const bool changed = !task_.intro.fontDisplayPath.isEmpty() || !task_.intro.fontBodyPath.isEmpty();
    task_.intro.fontDisplayPath.clear();
    task_.intro.fontBodyPath.clear();
    if (!changed) {
        return;
    }
    emit introChanged();
    savePreferences();
    if (engine() != nullptr) {
        engine()->refreshIntroState();
    }
}

void ExportSession::setIntroSoundIndex(int index)
{
    const QVariantList options = introSoundOptions();
    if (index < 0 || index >= options.size()) {
        return;
    }
    setIntroSoundFileName(
        options.at(index).toMap().value(QStringLiteral("fileName")).toString());
}

void ExportSession::setIntroSoundFileName(const QString& fileName)
{
    const QString normalized = miacode::preview_sfx::normalizeIntroSoundFileName(fileName);
    if (task_.introSoundFileName == normalized) {
        return;
    }
    task_.introSoundFileName = normalized;
    miacode::preview_sfx::setSelectedIntroSoundFileName(normalized);
    // The window reloads the SFX bank and persists in reaction to this.
    appearance_->setIntroSoundFileName(normalized);
    emit introChanged();
}

void ExportSession::setExportStartSeconds(double value)
{
    if (!qIsFinite(value)) {
        return;
    }
    const double end = exportEndSeconds();
    const double maximumStart = qMax(0.0, end - minimumExportRangeSeconds());
    setExportRangeSeconds(qBound(0.0, value, maximumStart), end);
}

void ExportSession::setExportEndSeconds(double value)
{
    if (!qIsFinite(value)) {
        return;
    }
    const double start = task_.exportStartSeconds;
    const double minimumEnd = start + minimumExportRangeSeconds();
    setExportRangeSeconds(start, qBound(minimumEnd, value, chartDurationSeconds_));
}

void ExportSession::setBatchOutputDirectory(const QString& path)
{
    if (batchOutputDirectory_ == path) {
        return;
    }
    batchOutputDirectory_ = path;
    QJsonObject preferences = miacode::app_preferences::loadDialogPreferences();
    preferences.insert(QLatin1String(kBatchOutputDirectoryKey), path);
    miacode::app_preferences::saveDialogPreferences(preferences);
    emit batchChanged();
}

} // namespace miacode::ui
