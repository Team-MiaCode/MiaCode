#pragma once

#include <QJsonObject>
#include <functional>
#include <utility>

#include "app/services/PreferenceDocument.h"
#include "app/services/PreferenceVersionGuard.h"

namespace miacode::app_preferences {

// Bump when a stored preference needs to be reinterpreted. Absent means a
// pre-versioned (v1) object. Migrations run in migrateDialogPreferences().
inline constexpr int kDialogPreferencesSchemaVersion = 2;

// Bring a loaded preferences object up to the current schema. Kept centralized
// so every load site (single dialog + embedded batch settings panel) migrates
// identically. In-memory only; the bumped version is persisted on the next
// saveDialogPreferences().
inline QJsonObject migrateDialogPreferences(QJsonObject preferences)
{
    if (!supportsNumericVersion(preferences, QStringLiteral("schema_version"), kDialogPreferencesSchemaVersion)) return preferences;
    const int version = preferences.value(QStringLiteral("schema_version")).toInt(1);

    // v1 -> v2: the intro card type gained an "Auto" default that detects SD/DX
    // per chart. Legacy objects could only persist a concrete "Standard"/"DX",
    // which would otherwise pin the new default. Drop it so the combo falls back
    // to its "auto" construction default.
    if (version < 2) {
        preferences.remove(QStringLiteral("intro_card_type"));
    }

    preferences.insert(QStringLiteral("schema_version"), kDialogPreferencesSchemaVersion);
    return preferences;
}

// Owns the whole stored section; callers load before modifying task fields so
// unrelated HUD settings and future sibling fields survive the round trip.
class VideoExportPreferences {
public:
    using Reader = std::function<QJsonObject()>;
    using Writer = std::function<bool(const QJsonObject&)>;
    VideoExportPreferences(Reader reader, Writer writer)
        : reader_(std::move(reader)), writer_(std::move(writer)) {}
    QJsonObject load() const { return migrateDialogPreferences(reader_()); }
    bool save(QJsonObject preferences)
    {
        if (!allowVersionWrite(reader_(), QStringLiteral("schema_version"), kDialogPreferencesSchemaVersion, QStringLiteral("video-export latest"))
            || !allowVersionWrite(preferences, QStringLiteral("schema_version"), kDialogPreferencesSchemaVersion, QStringLiteral("video-export incoming"))) return false;
        preferences.insert(QStringLiteral("schema_version"), kDialogPreferencesSchemaVersion);
        return writer_(preferences);
    }
private:
    Reader reader_;
    Writer writer_;
};

inline VideoExportPreferences& videoExportPreferences()
{
    static VideoExportPreferences preferences(
        [] { return PreferenceDocument::loadPreferencesObject().value(QStringLiteral("app")).toObject()
                        .value(QStringLiteral("video_export")).toObject(); },
        [](const QJsonObject& section) {
            QJsonObject root = PreferenceDocument::loadPreferencesObject();
            QJsonObject app = root.value(QStringLiteral("app")).toObject();
            app.insert(QStringLiteral("video_export"), section);
            root.insert(QStringLiteral("app"), app);
            return PreferenceDocument::savePreferencesObject(root);
        });
    return preferences;
}

inline QJsonObject loadDialogPreferences() { return videoExportPreferences().load(); }
inline bool saveDialogPreferences(const QJsonObject& preferences)
{
    return videoExportPreferences().save(preferences);
}

} // namespace miacode::app_preferences
