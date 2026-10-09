#pragma once

#include "app/services/PreferencesStore.h"
#include <QObject>
#include <QJsonObject>

namespace miacode::ui { class WorkbenchSettings; }
namespace miacode::android {
class MobilePreview;
class MobileTimeline;

class MobilePreferencesStore final : public QObject, public PreferencesStore {
    Q_OBJECT
    Q_PROPERTY(bool panelsSwapped READ workspacePanelsSwapped NOTIFY panelsSwappedChanged)
public:
    MobilePreferencesStore(ui::WorkbenchSettings& settings, MobilePreview& preview,
        MobileTimeline& timeline, QObject* parent = nullptr);
    // Must run before the first QMediaPlayer/decoder is constructed.
    static void preparePlatformDefaultsAndDecoder();
    int editorTextFontSize() const override { return pointSize_; }
    double editorLineSpacingFactor() const override { return spacing_; }
    bool editorHalfWidthInputEnabled() const override { return halfWidth_; }
    bool editorAutoCompletionEnabled() const override { return completion_; }
    bool editorImeInputDisabled() const override { return imeDisabled_; }
    void applyEditorTextFontSize(int value, bool persist) override;
    void applyEditorLineSpacingFactor(double value, bool persist) override;
    void applyEditorHalfWidthInputEnabled(bool value, bool persist) override;
    void applyEditorAutoCompletionEnabled(bool value, bool persist) override;
    void applyEditorImeInputDisabled(bool value, bool persist) override;
    PreviewCanvasFrameRateMode previewCanvasFrameRateMode() const override { return canvasMode_; }
    PreviewCanvasFrameRateMode previewStageMediaFrameRateMode() const override { return pvMode_; }
    PreviewCanvasFrameRateMode timelineFrameRateMode() const override { return timelineMode_; }
    double previewCanvasRefreshRate() const override;
    void setPreviewCanvasFrameRateMode(PreviewCanvasFrameRateMode mode, bool persist) override;
    void setPreviewStageMediaFrameRateMode(PreviewCanvasFrameRateMode mode, bool persist) override;
    void setTimelineFrameRateMode(PreviewCanvasFrameRateMode mode, bool persist) override;
    bool videoDecodePrefersSoftware() const override { return software_; }
    bool videoDecodeRequiresRestart() const override { return true; }
    void setVideoDecodePrefersSoftware(bool value, bool persist) override;
    bool workspacePanelsSwapped() const override { return swapped_; }
    void setWorkspacePanelsSwapped(bool value, bool persist) override;
signals:
    void panelsSwappedChanged();
    void refreshRateChanged();
private:
    void applyEditor(bool persist);
    void applyFrameRates();
    ui::WorkbenchSettings& settings_;
    MobilePreview& preview_;
    MobileTimeline& timeline_;
    int pointSize_ = 13;
    double spacing_ = 1;
    bool halfWidth_ = true, completion_ = true, imeDisabled_ = false;
    bool software_ = false, swapped_ = false;
    PreviewCanvasFrameRateMode canvasMode_ = PreviewCanvasFrameRateMode::Fps60;
    PreviewCanvasFrameRateMode pvMode_ = PreviewCanvasFrameRateMode::Fps30;
    PreviewCanvasFrameRateMode timelineMode_ = PreviewCanvasFrameRateMode::Fps60;
};
}
