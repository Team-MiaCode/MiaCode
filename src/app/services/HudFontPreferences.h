#pragma once

#include "app/services/VideoExportPreferences.h"
#include "core/scene/PreviewHudState.h"
#include "common/DebugLog.h"

#include <QFileInfo>

namespace miacode::hud_preferences {

inline QString areaKey(preview::scene::PreviewHudFontArea area)
{
    static const char* keys[] = {"chart_info", "timestamp", "center_display", "object_stats", "debug_info"};
    return QString::fromLatin1(keys[static_cast<int>(area)]);
}

inline QString validPath(const QString& path)
{
    if (path.isEmpty()) return {};
    const QFileInfo file(path);
    const QString suffix = file.suffix().toLower();
    return file.isFile() && (suffix == QLatin1String("ttf") || suffix == QLatin1String("otf"))
        ? file.absoluteFilePath() : QString();
}

inline QJsonObject migrateSection(QJsonObject section)
{
    if (!app_preferences::supportsNumericVersion(section, QStringLiteral("schema_version"), app_preferences::kDialogPreferencesSchemaVersion)) return section;
    const QString legacyKey = QStringLiteral("hud_font_path");
    if (!section.contains(legacyKey)) return section;
    const QString legacy = validPath(section.value(legacyKey).toString());
    if (!legacy.isEmpty()) {
        QJsonObject paths = section.value(QStringLiteral("hud_font_paths")).toObject();
        for (const auto& choice : preview::scene::previewHudFontAreaChoices()) {
            const QString key = areaKey(choice.area);
            // A present empty value is an explicit bundled-default choice.
            if (!paths.contains(key)) paths.insert(key, legacy);
        }
        section.insert(QStringLiteral("hud_font_paths"), paths);
    }
    section.remove(legacyKey);
    return section;
}

inline preview::scene::PreviewHudFontSettings fromSection(const QJsonObject& section)
{
    preview::scene::PreviewHudFontSettings settings;
    const QJsonObject paths = section.value(QStringLiteral("hud_font_paths")).toObject();
    for (const auto& choice : preview::scene::previewHudFontAreaChoices()) {
        const QString key = areaKey(choice.area);
        settings.paths[static_cast<int>(choice.area)] = validPath(paths.value(key).toString());
    }
    return settings;
}

inline QJsonObject section()
{
    QJsonObject root = PreferenceDocument::loadPreferencesObject();
    QJsonObject app = root.value(QStringLiteral("app")).toObject();
    const QJsonObject previous = app.value(QStringLiteral("video_export")).toObject();
    const QJsonObject migrated = migrateSection(previous);
    if (migrated != previous) {
        app.insert(QStringLiteral("video_export"), migrated);
        root.insert(QStringLiteral("app"), app);
        // Repository accepts the migrated runtime value even if disk save fails.
        const bool saved = PreferenceDocument::savePreferencesObject(root);
        debug_log::appendLine(debug_log::Channel::Runtime, QStringLiteral("hud_font_migration"),
            QStringLiteral("legacy-key-removed saved=%1").arg(saved), true,
            saved ? debug_log::Level::Info : debug_log::Level::Warn);
    }
    return migrated;
}

inline preview::scene::PreviewHudFontSettings load()
{
    return fromSection(section());
}

inline QJsonObject withPath(QJsonObject video, preview::scene::PreviewHudFontArea area, const QString& path)
{
    if (!app_preferences::supportsNumericVersion(video, QStringLiteral("schema_version"), app_preferences::kDialogPreferencesSchemaVersion)) return video;
    video = migrateSection(video);
    QJsonObject paths = video.value(QStringLiteral("hud_font_paths")).toObject();
    paths.insert(areaKey(area), path.isEmpty() ? QString() : QFileInfo(path).absoluteFilePath());
    video.insert(QStringLiteral("hud_font_paths"), paths);
    return video;
}

inline bool setPath(preview::scene::PreviewHudFontArea area, const QString& path)
{
    QJsonObject video = app_preferences::videoExportPreferences().load();
    if (!app_preferences::allowVersionWrite(video, QStringLiteral("schema_version"), app_preferences::kDialogPreferencesSchemaVersion, QStringLiteral("hud-font"))) return false;
    return app_preferences::videoExportPreferences().save(withPath(video, area, path));
}

} // namespace miacode::hud_preferences
