#pragma once

#include "app/services/PreferenceDocument.h"
#include "app/services/ProjectPreferences.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

namespace miacode::file_dialog_preferences {

enum class Purpose {
    IntroSound, IntroFont, CoverFont, HudFont, CoverLayout, PvBatchRoot,
};

enum class ChartPurpose {
    Image, Video, MetadataAudio, CoverImage, CoverBackground,
};

inline QString key(Purpose purpose)
{
    switch (purpose) {
    case Purpose::IntroSound: return QStringLiteral("intro_sound");
    case Purpose::IntroFont: return QStringLiteral("intro_font");
    case Purpose::CoverFont: return QStringLiteral("cover_font");
    case Purpose::HudFont: return QStringLiteral("hud_font");
    case Purpose::CoverLayout: return QStringLiteral("cover_layout");
    case Purpose::PvBatchRoot: return QStringLiteral("pv_batch_root");
    }
    return {};
}

inline QString key(ChartPurpose purpose)
{
    switch (purpose) {
    case ChartPurpose::Image: return QStringLiteral("chart_image");
    case ChartPurpose::Video: return QStringLiteral("chart_video");
    case ChartPurpose::MetadataAudio: return QStringLiteral("metadata_audio");
    case ChartPurpose::CoverImage: return QStringLiteral("cover_image");
    case ChartPurpose::CoverBackground: return QStringLiteral("cover_background");
    }
    return {};
}

inline QString validDirectory(const QString& path)
{
    if (path.trimmed().isEmpty()) return {};
    const QFileInfo info(QDir::fromNativeSeparators(path.trimmed()));
    return info.isAbsolute() && info.isDir() ? info.absoluteFilePath() : QString();
}

inline QString storedDirectory(Purpose purpose)
{
    const auto app = PreferenceDocument::loadPreferencesObject().value(QStringLiteral("app")).toObject();
    const auto history = app.value(QStringLiteral("file_dialogs")).toObject();
    return validDirectory(history.value(key(purpose)).toString());
}

inline QString recentDirectory(Purpose purpose, const QString& fallback = {})
{
    for (const auto& candidate : {storedDirectory(purpose), fallback, QDir::homePath()}) {
        const QString directory = validDirectory(candidate);
        if (!directory.isEmpty()) return directory;
    }
    return {};
}

inline bool rememberDirectory(Purpose purpose, const QString& path)
{
    const QString directory = validDirectory(path);
    if (directory.isEmpty()) return false; // Cancel and invalid paths leave history intact.
    auto root = PreferenceDocument::loadPreferencesObject();
    auto app = root.value(QStringLiteral("app")).toObject();
    auto history = app.value(QStringLiteral("file_dialogs")).toObject();
    if (history.value(key(purpose)).toString() == directory) return true;
    history.insert(key(purpose), directory);
    app.insert(QStringLiteral("file_dialogs"), history);
    root.insert(QStringLiteral("app"), app);
    return PreferenceDocument::savePreferencesObject(root);
}

inline bool rememberFile(Purpose purpose, const QString& path)
{
    if (path.trimmed().isEmpty() || !QFileInfo(path).isAbsolute()) return false;
    return rememberDirectory(purpose, QFileInfo(path).absolutePath());
}

inline QString storedChartDirectory(ChartPurpose purpose, const QString& chartFile)
{
    const auto project = project_preferences::load(chartFile);
    return validDirectory(project.value(QStringLiteral("file_dialogs")).toObject().value(key(purpose)).toString());
}

inline QString chartDirectory(ChartPurpose purpose, const QString& chartFile)
{
    const QString remembered = storedChartDirectory(purpose, chartFile);
    if (!remembered.isEmpty()) return remembered;
    const QString directory = chartFile.isEmpty() ? QString() : validDirectory(QFileInfo(chartFile).absolutePath());
    return directory.isEmpty() ? QDir::homePath() : directory;
}

inline bool rememberChartFile(ChartPurpose purpose, const QString& chartFile, const QString& path)
{
    if (chartFile.isEmpty() || path.isEmpty() || !QFileInfo(path).isAbsolute()) return false;
    const QString directory = validDirectory(QFileInfo(path).absolutePath());
    if (directory.isEmpty()) return false;
    auto project = project_preferences::load(chartFile);
    auto history = project.value(QStringLiteral("file_dialogs")).toObject();
    if (history.value(key(purpose)).toString() == directory) return true;
    history.insert(key(purpose), directory);
    project.insert(QStringLiteral("file_dialogs"), history);
    return project_preferences::save(chartFile, project);
}

// Shared document policy: persisted document location, current chart, then the
// same first-use fallbacks as the v1 document host.
inline QString documentDirectory(const QString& lastDirectory, const QString& currentFile)
{
    const QString chartDirectory = currentFile.isEmpty() ? QString() : QFileInfo(currentFile).absolutePath();
    for (const auto& candidate : {lastDirectory, chartDirectory,
                                 QCoreApplication::applicationDirPath(), QDir::currentPath()}) {
        const QString directory = validDirectory(candidate);
        if (!directory.isEmpty()) return directory;
    }
    return {};
}

} // namespace miacode::file_dialog_preferences
