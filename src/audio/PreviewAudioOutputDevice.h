#pragma once

#include <QMetaType>
#include <QString>

namespace miacode::preview_audio {

struct PreviewAudioOutputDeviceInfo {
    // Empty id represents "System Default". For concrete endpoints this carries
    // a stable platform identifier (WASAPI endpoint GUID on Windows, Core Audio
    // UID on macOS, or ALSA/PipeWire driver/name on Linux).
    QString id;
    QString name;
    QString driver;
    bool isDefault = false;
    bool isEnabled = true;
};

}  // namespace miacode::preview_audio

Q_DECLARE_METATYPE(miacode::preview_audio::PreviewAudioOutputDeviceInfo)
