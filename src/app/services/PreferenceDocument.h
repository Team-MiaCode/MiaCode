#pragma once

#include <QJsonObject>
#include <QMutex>
#include <QVector>
#include <QString>
#include <QStringList>

namespace PreferenceDocument {

enum class LanguagePreference {
    System,
    English,
    Chinese,
    Japanese,
};

enum class ThemePreference {
    System,
    Light,
    Dark,
    // Read/write compatibility for the short-lived single-palette draft.
    // Normalized preferences migrate this value to Dark + legacy dark palette.
    Legacy,
};

enum class ThemePalette {
    Light,
    Dark,
    Legacy,
    LegacyLight,
};

struct LanguageOption {
    QString id;
    QString label;
    bool builtIn = false;
};

// One in-memory document for a repository lifetime. External file changes are
// observed by a new repository, never by ordinary UI reads. Access is serialized;
// snapshots are values suitable for passing to workers. While `path` is missing
// the first existing migration source is imported; sources are never written.
class Repository {
public:
    explicit Repository(QString path, QStringList migrationSources = {});
    QJsonObject snapshot();
    QString storedSchema();
    QString migrationStatus();
    bool replace(const QJsonObject& root);
    bool flush();
    bool isDirty();

private:
    void initializeLocked();
    bool flushLocked();
    QMutex mutex_;
    QString path_;
    QStringList migrationSources_;
    QString storedSchema_;
    QString migrationStatus_;
    QJsonObject root_;
    bool initialized_ = false;
    bool dirty_ = false;
    bool unsupportedSchema_ = false;
    bool sourceReadBlocked_ = false;
    QString unreadSourcePath_;
};

LanguagePreference resolvedLanguage();
LanguagePreference preferredLanguage();
void setPreferredLanguage(LanguagePreference preference);
QString preferredLanguageToken();
void setPreferredLanguageToken(const QString& token);
QString resolvedLanguageToken();
QVector<LanguageOption> availableLanguageOptions();
bool isLanguageAvailable(const QString& token);
bool ensurePreferredLanguageAvailable();
ThemePreference preferredTheme();
void setPreferredTheme(ThemePreference preference);
QString themePreferenceToken(ThemePreference preference);
ThemePreference themePreferenceFromToken(const QString& token);
ThemePalette preferredLightTheme();
void setPreferredLightTheme(ThemePalette palette);
ThemePalette preferredDarkTheme();
void setPreferredDarkTheme(ThemePalette palette);
QString themePaletteToken(ThemePalette palette);
ThemePalette themePaletteFromToken(const QString& token);
bool themePaletteIsDark(ThemePalette palette);
QString preferencesFilePath();
QString currentPreferencesSchema();
QString storedPreferencesSchema();
QString migrationStatus();
QJsonObject loadPreferencesObject();
// Applies the new runtime value even if disk persistence returns false.
bool savePreferencesObject(const QJsonObject& root);
QJsonObject normalizePreferencesObject(const QJsonObject& root);
QString themeTokenFromPreferencesObject(const QJsonObject& root);
void setThemeTokenInPreferencesObject(QJsonObject* root, const QString& token);

}  // namespace PreferenceDocument
