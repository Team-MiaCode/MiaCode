#pragma once

#include "export/video_export/EncoderProbeCache.h"
#include "common/DebugLog.h"
#include <QSettings>

#include <mutex>

namespace miacode::runtime {

// Encoder probing is a machine cache, separate from portable user preferences.
class SettingsEncoderProbeCache final : public miacode::video_export::EncoderProbeCache {
public:
    QString preferredHardwareEncoder() const override
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        if (loaded_) {
            return cachedCodec_;
        }
        QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                           QStringLiteral("MiaCode"), QStringLiteral("VideoExportRuntime"));
        const QString codec = settings.value(key()).toString().trimmed();
        if (settings.status() != QSettings::NoError) {
            miacode::debug_log::appendLine(miacode::debug_log::Channel::Export,
                QStringLiteral("encoder_probe_cache"),
                QStringLiteral("encoder_probe_cache_read_failed path=%1 status=%2")
                    .arg(settings.fileName()).arg(static_cast<int>(settings.status())),
                true, miacode::debug_log::Level::Error);
            cachedCodec_.clear();
            loaded_ = true;
            return {};
        }
        cachedCodec_ = codec;
        loaded_ = true;
        return cachedCodec_;
    }
    void rememberPreferredHardwareEncoder(const QString& codec) override
    {
        const QString normalized = codec.trimmed();
        QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                           QStringLiteral("MiaCode"), QStringLiteral("VideoExportRuntime"));
        settings.setValue(key(), normalized);
        settings.sync();
        if (settings.status() != QSettings::NoError) {
            miacode::debug_log::appendLine(miacode::debug_log::Channel::Export,
                QStringLiteral("encoder_probe_cache"), QStringLiteral("encoder_probe_cache_write_failed path=%1 status=%2")
                                .arg(settings.fileName()).arg(static_cast<int>(settings.status())),
                true, miacode::debug_log::Level::Error);
            return;
        }
        const std::lock_guard<std::mutex> lock(mutex_);
        cachedCodec_ = normalized;
        loaded_ = true;
    }
private:
    static QString key()
    {
        return QStringLiteral("video_export/runtime_probe/preferred_hardware_encoder");
    }

    mutable std::mutex mutex_;
    mutable bool loaded_ = false;
    mutable QString cachedCodec_;
};

} // namespace miacode::runtime
