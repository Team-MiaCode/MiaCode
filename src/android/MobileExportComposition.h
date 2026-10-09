#pragma once

#include "MobileVideoExport.h"
#include "app/services/ExportEngine.h"
#include "app/services/PreviewSurface.h"
#include "app/services/PreviewAppearanceState.h"
#include "app/services/ShellNotifications.h"
#include "app/services/UiRequestService.h"
#include "app/services/JobProgressService.h"
#include "app/ui/export/ExportSession.h"
#include "app/ui/preview/PreviewSettingsModel.h"
#include "app/ui/preview/AudioSettingsModel.h"
#include "MobileAudioAudition.h"
#include <QFileInfo>

namespace miacode::android {

// The Android implementation of the existing v2 page contracts. The original
// settings session and QML forms remain the UI owners.
class MobileExportComposition final : public QObject, public ExportEngine, public PreviewSurface {
    Q_OBJECT
    Q_PROPERTY(QObject* session READ session CONSTANT)
    Q_PROPERTY(QObject* settings READ settings CONSTANT)
    Q_PROPERTY(QObject* audioSettings READ audioSettingsModel CONSTANT)
    Q_PROPERTY(QObject* requests READ requests CONSTANT)
    Q_PROPERTY(QObject* progress READ progress CONSTANT)
public:
    MobileExportComposition(AndroidDocumentSession&, MobilePreview&, MobileVideoExport&, QObject* parent = nullptr);
    ~MobileExportComposition() override;
    QObject* session() { return &session_; }
    QObject* settings() { return &settings_; }
    QObject* audioSettingsModel() { return &audioSettingsModel_; }
    QObject* requests() { return &requests_; }
    QObject* progress() { return &progress_; }
    bool exportActive() const { return batchRunning_ || exporter_.running(); }
    Q_INVOKABLE void enter() { session_.enter(document_.activeDifficulty()); }
    Q_INVOKABLE void leave() { session_.leave(); }

    VideoExportTask buildSeedTask(int id) override;
    void applySharedTaskSettings(const VideoExportTask&) override;
    bool startAudition(int, const VideoExportTask&) override;
    void stopAudition() override;
    bool launchVideoExport(const VideoExportTask&, int, QString*) override;
    bool launchBatchExport(const VideoExportTask&, const QStringList&, const QList<int>&,
                           const QString&, BatchResult*, const BatchCallbacks&, QString*) override;
    void cancelVideoExport() override { exporter_.cancel(); }
    QList<int> difficultyIds() const override { return document_.workspace().document().difficultyIds(); }
    QString difficultyChartText(int id) const override;
    int lastOpenedDifficultyId() const override { return document_.activeDifficulty(); }
    MuriRenderOptions muriRenderOptions() const override { return preview_.muriRenderOptions(); }
    double currentAudioClockSecond() const override { return preview_.positionSeconds(); }
    void refreshIntroState() override;

    bool playing() const override { return preview_.playing(); }
    PlaybackTransportState playbackTransportState() const override;
    double positionSeconds() const override { return preview_.positionSeconds(); }
    double durationSeconds() const override { return preview_.durationSeconds(); }
    double lowerBoundSeconds() const override { return preview_.lowerBoundSeconds(); }
    double playbackRate() const override { return preview_.rate(); }
    QString playbackRateLabel() const override { return QString::number(preview_.rate()) + "x"; }
    QObject* previewRuntimeObject() const override { return preview_.runtime(); }
    QObject* stageMediaHostObject() const override { return preview_.mediaHost(); }
    double canvasAspectRatio() const override { return preview_.canvasAspectRatio(); }
    QStringList statsTexts() const override;
    RenderMode muriRenderMode() const override { return preview_.muriRenderOptions().renderMode; }
    void setMuriRenderMode(RenderMode) override;
    void toggleMuriRenderMode() override { setMuriRenderMode(preview_.muriCheckEnabled() ? RenderMode::Native : RenderMode::MaimuriDxStyle); }
    int muriHandRadiusPx() const override { return preview_.muriHandRadiusPx(); }
    void setMuriHandRadiusPx(int value) override { preview_.setMuriHandRadiusPx(value); }
    int muriTapOnSlideThresholdMs() const override { return preview_.muriTapOnSlideThresholdMs(); }
    void setMuriTapOnSlideThresholdMs(int value) override { preview_.setMuriTapOnSlideThresholdMs(value); }
    QStringList availableSkinDirectoryNames() const override;
    QString skinDisplayName(const QString& name) const override;
    QString resolveSkinDir() const override { return preview_.sceneRuntime().skinDirectory(); }
    QString resolveSkinRootDir() const override;
    QString resolveCustomOutlineDir() const override;
    QStringList availableCustomOutlineFileNames() const override;
    QString customOutlineDisplayName(const QString& name) const override { return QFileInfo(name).completeBaseName(); }
    QString currentCustomOutlineFileName() const override { return customOutline_; }
    void applyOutlineVariant(PreviewOutlineVariant, bool, bool) override;
    void applyCustomOutlineFileName(const QString&, bool) override;
    QVariantMap renderSettings() const override;
    void setRenderSetting(const QString&, const QVariant&) override;
    void refreshSurfaces() override;
    void applySfxLevels() override { preview_.applyAudioSettings(audioSettings_); }
    void prepareForShutdown() override { preview_.stop(); exporter_.cancel(); }
    PreviewAudioSettings audioSettings() const override { return audioSettings_; }
    void applyAudioSettings(const PreviewAudioSettings&) override;
    void saveAudioSettingsAsSoftwareDefault() override;
    void restoreAudioSettingsFromSoftwareDefault() override;
private:
    void updateIntroFrame();
    void applyEffectiveOutline();
    void savePreviewPreferences();
    void restorePreviewPreferences();
    void loadProjectAudioPreferences();
    void saveProjectAudioPreferences();
    void applyRuntimeAudioSettings(const PreviewAudioSettings&);
    AndroidDocumentSession& document_;
    MobilePreview& preview_;
    MobileVideoExport& exporter_;
    ShellNotifications notifications_;
    UiRequestService requests_;
    JobProgressService progress_;
    PreviewAppearanceState appearance_;
    ExportEngine* engineSlot_ = this;
    PreviewSurface* surfaceSlot_ = this;
    ui::ExportSession session_;
    ui::PreviewSettingsModel settings_;
    MobileAudioAudition audioAudition_;
    ui::AudioSettingsModel audioSettingsModel_;
    PreviewAudioSettings audioSettings_;
    bool restoringPreferences_ = true;
    bool editedAudioWithoutProject_ = false;
    bool breakSlideTailCheerMutedPreference_ = false;
    QString audioProjectPath_;
    QString customOutline_;
    bool forceLabeledJudgeLineWhenPaused_ = true;
    bool batchRunning_ = false;
    int previousDifficulty_ = 0;
    quint64 jobToken_ = 0;
};
}
