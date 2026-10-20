#pragma once

#include "export/video_export/EncoderProbeCache.h"
#include "common/DebugLog.h"
#include <QSettings>

namespace miacode::runtime {

// Encoder probing is a machine cache, separate from portable user preferences.
class SettingsEncoderProbeCache final : public miacode::video_export::EncoderProbeCache {
public:
    QString preferredHardwareEncoder() const override
    {
        QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                           QStringLiteral("MiaCode"), QStringLiteral("VideoExportRuntime"));
        const QString codec = settings.value(key()).toString().trimmed();
        if (settings.status() != QSettings::NoError) {
            miacode::debug_log::appendLine(miacode::debug_log::Channel::Export,
                QStringLiteral("encoder_probe_cache"),
                QStringLiteral("encoder_probe_cache_read_failed path=%1 status=%2")
                    .arg(settings.fileName()).arg(static_cast<int>(settings.status())),
                true, miacode::debug_log::Level::Error);
            return {};
        }
        return codec;
    }
    void rememberPreferredHardwareEncoder(const QString& codec) override
    {
        QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                           QStringLiteral("MiaCode"), QStringLiteral("VideoExportRuntime"));
        settings.setValue(key(), codec);
        settings.sync();
        if (settings.status() != QSettings::NoError) {
            miacode::debug_log::appendLine(miacode::debug_log::Channel::Export,
                QStringLiteral("encoder_probe_cache"), QStringLiteral("encoder_probe_cache_write_failed path=%1 status=%2")
                                .arg(settings.fileName()).arg(static_cast<int>(settings.status())),
                true, miacode::debug_log::Level::Error);
        }
    }
private:
    static QString key()
    {
        return QStringLiteral("video_export/runtime_probe/preferred_hardware_encoder");
    }
};

} // namespace miacode::runtime
