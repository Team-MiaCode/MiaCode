#pragma once

#include "core/video/PreviewRenderSettings.h"

#include <QString>

namespace miacode {

// 播放协调器通过此端口管理预览媒体、资源设置与关闭流程。
class PlaybackPreviewPort
{
public:
    virtual ~PlaybackPreviewPort() = default;

    virtual void ensurePreviewStageMediaRouteInitialized() = 0;
    virtual void syncPreviewStageMediaRouteChartPath(
        const QString& chartPath,
        const QString& trackPath,
        double pausedSecond,
        const QString& chartVideoOverridePath) = 0;
    virtual void schedulePreviewSubsystemWarmup() = 0;
    virtual void applyPreviewAudioSettingsToRuntime() = 0;
    virtual void loadProjectAudioPreferences() = 0;
    virtual void applyEffectivePreviewOutlineVariantToCanvas() = 0;
    virtual void preparePreviewForShutdown() = 0;
    virtual QString resolvePreviewSkinDir() const = 0;
    virtual void applyPreviewOutlineVariant(PreviewOutlineVariant variant, bool useAutoSelection,
                                             bool persistState) = 0;
    virtual QString currentPreviewCustomOutlineFileName() const = 0;
    virtual void applyPreviewCustomOutlineFileName(const QString& fileName, bool persistState) = 0;
};

}  // namespace miacode
