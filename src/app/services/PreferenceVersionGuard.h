#pragma once

#include "common/DebugLog.h"
#include <QJsonObject>

namespace miacode::app_preferences {

inline bool supportsNumericVersion(const QJsonObject& object, const QString& key, int current)
{
    if (!object.contains(key)) return true;
    const QJsonValue value = object.value(key);
    const double version = value.toDouble(-1);
    return value.isDouble() && version >= 1 && version <= current && version == int(version);
}

inline bool allowVersionWrite(const QJsonObject& object, const QString& key, int current,
                              const QString& context)
{
    if (supportsNumericVersion(object, key, current)) return true;
    debug_log::appendLine(debug_log::Channel::Runtime, QStringLiteral("preferences"),
        context + QStringLiteral(" unsupported-version save-blocked"), true, debug_log::Level::Warn);
    return false;
}

} // namespace miacode::app_preferences
