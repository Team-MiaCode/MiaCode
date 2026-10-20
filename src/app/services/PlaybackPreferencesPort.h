#pragma once

#include "audio/PreviewAudioSettings.h"

#include <QString>
#include <QVariant>
#include <QVariantMap>

namespace miacode {

// 播放协调器通过此端口读写偏好与持久化状态。
class PlaybackPreferencesPort
{
public:
    virtual ~PlaybackPreferencesPort() = default;

    virtual void savePortableState() const = 0;
    virtual void setPreviewRenderSetting(const QString& key, const QVariant& value) = 0;
    virtual QVariantMap previewRenderSettings() const = 0;
    virtual void loadProjectRenderState() = 0;
    virtual void savePreviewAudioSettingsAsSoftwareDefault() = 0;
    virtual void restorePreviewAudioSettingsFromSoftwareDefault() = 0;
    virtual void applyPreviewAudioSettingsFromUi(const PreviewAudioSettings& settings) = 0;
    virtual void setLastOpenDirectory(const QString& pathOrDir) = 0;
};

}  // namespace miacode
