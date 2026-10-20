#pragma once

#include "app/services/PreferenceDocument.h"

#include <QDir>
#include <QFileInfo>

namespace miacode::app_preferences {

inline QString fontLibraryDirectory()
{
    const QString preferencesPath = PreferenceDocument::preferencesFilePath();
    if (preferencesPath.isEmpty() || !QDir::isAbsolutePath(preferencesPath)) return {};
    return QFileInfo(preferencesPath).absoluteDir().filePath(QStringLiteral("fonts"));
}

} // namespace miacode::app_preferences
