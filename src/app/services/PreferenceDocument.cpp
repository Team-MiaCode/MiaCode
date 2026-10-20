#include "app/services/PreferenceDocument.h"
#include "app/services/PreferenceJsonFile.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QLocale>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QStringList>

#include <utility>

namespace {

// Document schema and setup completion are independent.
constexpr auto kPreferencesSchema = "miacode_preferences_v4";
constexpr auto kUiSectionKey = "ui";
constexpr auto kAppSectionKey = "app";
constexpr auto kPreviewSectionKey = "preview";
constexpr auto kLanguageKey = "language";
constexpr auto kThemeKey = "theme";
constexpr auto kLightThemeKey = "light_theme";
constexpr auto kDarkThemeKey = "dark_theme";

QString configFilePath(const QString& fileName)
{
    const QString configRoot = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (configRoot.isEmpty()) {
        return QString();
    }
    return QDir(configRoot).filePath(fileName);
}

// 2.x keeps its own document beside the 1.x preferences.json. The 1.x files
// are read-only migration sources consulted only while this file is missing.
QString preferencesPath()
{
    return configFilePath(QStringLiteral("preferences_v2.json"));
}

QStringList migrationSourcePaths()
{
    QStringList sources;
    const QString v1 = configFilePath(QStringLiteral("preferences.json"));
    if (!v1.isEmpty()) sources.append(v1);
    sources.append(QDir(QCoreApplication::applicationDirPath()).filePath(".miacode_preferences.json"));
    return sources;
}

bool supportedPreferencesSchema(const QJsonObject& root)
{
    if (!root.contains(QStringLiteral("schema"))) return true;
    const QJsonValue value = root.value(QStringLiteral("schema"));
    if (!value.isString()) return false;
    for (int version = 1; version <= 4; ++version) {
        if (value.toString() == QStringLiteral("miacode_preferences_v%1").arg(version)) return true;
    }
    return false;
}

QJsonObject normalizedPreferencesRoot(const QJsonObject& raw)
{
    if (!supportedPreferencesSchema(raw)) return raw;
    QJsonObject normalized = raw;
    normalized.remove(kUiSectionKey);
    normalized.remove(kAppSectionKey);
    normalized.remove(QStringLiteral("extensions"));
    normalized.insert("schema", kPreferencesSchema);

    QJsonObject ui = raw.value(kUiSectionKey).toObject();
    if (!ui.contains(kLanguageKey) && raw.contains("ui_language")) {
        ui.insert(kLanguageKey, raw.value("ui_language").toString("system"));
    }
    if (!ui.contains(kLanguageKey)) {
        ui.insert(kLanguageKey, "system");
    }
    if (!ui.contains(kThemeKey) && raw.contains("ui_theme")) {
        ui.insert(kThemeKey, raw.value("ui_theme").toString("system"));
    }
    if (!ui.contains(kThemeKey) && raw.contains("theme")) {
        ui.insert(kThemeKey, raw.value("theme").toString("system"));
    }
    if (!ui.contains(kThemeKey)) {
        // Default theme for fresh installs is dark (user-voted default). Existing
        // users keep whatever they explicitly stored (incl. "system"/"light").
        ui.insert(kThemeKey, "dark");
    }
    // The short-lived single "legacy" option represented the old dark
    // palette and appearance mode in one value. Split it into the durable
    // mode + per-appearance palette model while retaining that selection.
    if (ui.value(kThemeKey).toString().trimmed().compare(
            QStringLiteral("legacy"), Qt::CaseInsensitive) == 0) {
        ui.insert(kThemeKey, QStringLiteral("dark"));
        ui.insert(kDarkThemeKey, QStringLiteral("legacy"));
    }
    // Normalize the paired-palette draft to four independent theme ids.
    if (ui.value(kLightThemeKey).toString().compare(
            QStringLiteral("v2"), Qt::CaseInsensitive) == 0) {
        ui.insert(kLightThemeKey, QStringLiteral("light"));
    }
    if (ui.value(kDarkThemeKey).toString().compare(
            QStringLiteral("v2"), Qt::CaseInsensitive) == 0) {
        ui.insert(kDarkThemeKey, QStringLiteral("dark"));
    }
    if (!ui.contains(kLightThemeKey)) {
        ui.insert(kLightThemeKey, QStringLiteral("light"));
    }
    if (!ui.contains(kDarkThemeKey)) {
        ui.insert(kDarkThemeKey, QStringLiteral("dark"));
    }
    // Migrate once at the document boundary so the editor runtime and shell
    // consume the same canonical value and defaults.
    ui.insert("editor_auto_completion", ui.value("editor_auto_completion").toBool(
        ui.value("editor_auto_close_brackets").toBool(true)));
    ui.insert("editor_half_width_input", ui.value("editor_half_width_input").toBool(true));
    ui.insert("editor_overwrite_mode", ui.value("editor_overwrite_mode").toBool(false));
    ui.insert("editor_ime_input_disabled", ui.value("editor_ime_input_disabled").toBool(true));
    ui.remove("editor_auto_close_brackets");
    normalized.insert(kUiSectionKey, ui);

    QJsonObject app = raw.value(kAppSectionKey).toObject();
    if (!app.contains("last_open_dir") && raw.contains("last_open_dir")) {
        app.insert("last_open_dir", raw.value("last_open_dir").toString());
    }
    if (!app.contains("last_track_path") && raw.contains("last_track_path")) {
        app.insert("last_track_path", raw.value("last_track_path").toString());
    }
    if (!app.contains("show_slide_tracks") && raw.contains("show_slide_tracks")) {
        app.insert("show_slide_tracks", raw.value("show_slide_tracks").toBool(true));
    }

    QJsonObject preview = app.value(kPreviewSectionKey).toObject();
    if (!preview.contains("show_judge_markers") && raw.contains("show_judge_markers")) {
        preview.insert("show_judge_markers", raw.value("show_judge_markers").toBool(false));
    }
    if (!preview.contains("show_touch_trail") && raw.contains("show_touch_trail")) {
        preview.insert("show_touch_trail", raw.value("show_touch_trail").toBool(false));
    }
    if (!preview.contains("background_brightness") && raw.contains("preview_background_brightness")) {
        preview.insert("background_brightness", raw.value("preview_background_brightness").toDouble(0.2));
    }
    if (!preview.contains("show_debug_info") && raw.contains("preview_show_debug_info")) {
        preview.insert("show_debug_info", raw.value("preview_show_debug_info").toBool(false));
    }
    QJsonObject audio = preview.value("audio").toObject();
    const bool canonicalTouchVolumePresent = audio.contains("touch_volume");
    if (raw.contains("preview_audio") && raw.value("preview_audio").isObject()) {
        const QJsonObject legacyAudio = raw.value("preview_audio").toObject();
        for (auto it = legacyAudio.constBegin(); it != legacyAudio.constEnd(); ++it) {
            if (!audio.contains(it.key())) {
                audio.insert(it.key(), it.value());
            }
        }
    }
    if (raw.contains("master_volume")
        || raw.contains("master_restore_volume")
        || raw.contains("bgm_volume")
        || raw.contains("sfx_volume")
        || raw.contains("answer_volume")
        || raw.contains("judge_volume")
        || raw.contains("ex_volume")
        || raw.contains("break_volume")
        || raw.contains("slide_volume")
        || raw.contains("touch_volume")
        || raw.contains("touchhold_volume")
        || raw.contains("break_slide_volume")
        || raw.contains("break_slide_restore_volume")
        || raw.contains("break_slide_tail_cheer_muted")
        || raw.contains("firework_volume")
        || raw.contains("hanabi_volume")) {
        if (!audio.contains("global_volume") && raw.contains("master_volume")) {
            audio.insert("global_volume", raw.value("master_volume").toDouble());
        }
        if (!audio.contains("global_restore_volume") && raw.contains("master_restore_volume")) {
            audio.insert("global_restore_volume", raw.value("master_restore_volume").toDouble());
        }
        if (raw.contains("bgm_volume")) {
            if (!audio.contains("track_volume")) {
                audio.insert("track_volume", raw.value("bgm_volume").toDouble());
            }
            if (!audio.contains("bgm_volume")) {
                audio.insert("bgm_volume", raw.value("bgm_volume").toDouble());
            }
        }
        if (!audio.contains("answer_volume") && raw.contains("answer_volume")) {
            audio.insert("answer_volume", raw.value("answer_volume").toDouble(raw.value("sfx_volume").toDouble()));
        }
        if (raw.contains("judge_volume")) {
            if (!audio.contains("tap_volume")) {
                audio.insert("tap_volume", raw.value("judge_volume").toDouble(raw.value("sfx_volume").toDouble()));
            }
            if (!audio.contains("judge_volume")) {
                audio.insert("judge_volume", raw.value("judge_volume").toDouble(raw.value("sfx_volume").toDouble()));
            }
        }
        if (!audio.contains("slide_volume") && raw.contains("slide_volume")) {
            audio.insert("slide_volume", raw.value("slide_volume").toDouble(raw.value("sfx_volume").toDouble()));
        }
        if (!audio.contains("break_volume") && raw.contains("break_volume")) {
            audio.insert("break_volume", raw.value("break_volume").toDouble(raw.value("sfx_volume").toDouble()));
        }
        if (!audio.contains("break_slide_volume") && raw.contains("break_slide_volume")) {
            audio.insert(
                "break_slide_volume",
                raw.value("break_slide_volume").toDouble(
                    raw.value("slide_volume").toDouble(raw.value("sfx_volume").toDouble())));
        }
        if (!audio.contains("break_slide_restore_volume") && raw.contains("break_slide_restore_volume")) {
            audio.insert(
                "break_slide_restore_volume",
                raw.value("break_slide_restore_volume").toDouble(
                    raw.value("break_slide_volume").toDouble(
                        raw.value("slide_volume").toDouble(raw.value("sfx_volume").toDouble()))));
        }
        if (!audio.contains("break_slide_tail_cheer_muted") && raw.contains("break_slide_tail_cheer_muted")) {
            audio.insert("break_slide_tail_cheer_muted", raw.value("break_slide_tail_cheer_muted").toBool(false));
        }
        if (!audio.contains("ex_volume") && raw.contains("ex_volume")) {
            audio.insert("ex_volume", raw.value("ex_volume").toDouble(raw.value("sfx_volume").toDouble()));
        }
        if (!audio.contains("touch_volume") && raw.contains("touch_volume")) {
            audio.insert("touch_volume", raw.value("touch_volume").toDouble(raw.value("sfx_volume").toDouble()));
        }
        if (raw.contains("touchhold_volume")) {
            const double touchHoldVolume = raw.value("touchhold_volume").toDouble(raw.value("sfx_volume").toDouble());
            if (!audio.contains("touchhold_volume")) {
                audio.insert("touchhold_volume", touchHoldVolume);
            }
            if (!canonicalTouchVolumePresent) {
                audio.insert("touch_volume", qMax(audio.value("touch_volume").toDouble(), touchHoldVolume));
            }
        }
        if (!audio.contains("firework_volume") && raw.contains("firework_volume")) {
            audio.insert("firework_volume", raw.value("firework_volume").toDouble(raw.value("sfx_volume").toDouble()));
        }
        if (!audio.contains("firework_volume") && raw.contains("hanabi_volume")) {
            audio.insert("firework_volume", raw.value("hanabi_volume").toDouble(raw.value("sfx_volume").toDouble()));
        }
    }
    if (!audio.isEmpty()) {
        preview.insert("audio", audio);
    }

    if (!preview.isEmpty()) {
        app.insert(kPreviewSectionKey, preview);
    }
    normalized.insert(kAppSectionKey, app);

    static const char* const legacyKeys[] = {
        "ui_language", "ui_theme", "theme", "last_open_dir", "last_track_path",
        "show_slide_tracks", "show_judge_markers", "show_touch_trail",
        "preview_background_brightness", "preview_show_debug_info", "preview_audio",
        "master_volume", "master_restore_volume", "bgm_volume", "sfx_volume",
        "answer_volume", "judge_volume", "ex_volume", "break_volume",
        "slide_volume", "touch_volume", "touchhold_volume", "break_slide_volume",
        "break_slide_restore_volume", "break_slide_tail_cheer_muted",
        "firework_volume", "hanabi_volume",
    };
    for (const char* key : legacyKeys) {
        normalized.remove(QString::fromLatin1(key));
    }
    return normalized;
}

QString normalizedLanguageToken(QString token)
{
    token = token.trimmed().toLower();
    token.replace('-', '_');
    return token;
}

PreferenceDocument::LanguagePreference parseLanguagePreference(const QString& raw)
{
    const QString token = normalizedLanguageToken(raw);
    if (token == "zh" || token == "zh_cn" || token == "zh_hans" || token == "zh_hans_cn" || token == "cn") {
        return PreferenceDocument::LanguagePreference::Chinese;
    }
    if (token == "ja" || token == "ja_jp" || token == "jp") {
        return PreferenceDocument::LanguagePreference::Japanese;
    }
    if (token == "en" || token == "en_us" || token == "en_gb") {
        return PreferenceDocument::LanguagePreference::English;
    }
    return PreferenceDocument::LanguagePreference::System;
}

QString languagePreferenceToken(PreferenceDocument::LanguagePreference preference)
{
    switch (preference) {
    case PreferenceDocument::LanguagePreference::English:
        return "en";
    case PreferenceDocument::LanguagePreference::Chinese:
        return "zh";
    case PreferenceDocument::LanguagePreference::Japanese:
        return "ja";
    case PreferenceDocument::LanguagePreference::System:
    default:
        return "system";
    }
}

PreferenceDocument::ThemePalette loadStoredThemePalette(const char* key)
{
    const QJsonObject ui = PreferenceDocument::loadPreferencesObject()
                               .value(kUiSectionKey).toObject();
    return PreferenceDocument::themePaletteFromToken(
        ui.value(QLatin1String(key)).toString(
            QLatin1String(key) == QLatin1String(kLightThemeKey)
                ? QStringLiteral("light")
                : QStringLiteral("dark")));
}

void saveStoredLanguagePreference(PreferenceDocument::LanguagePreference preference)
{
    QJsonObject root = PreferenceDocument::loadPreferencesObject();
    QJsonObject ui = root.value(kUiSectionKey).toObject();
    ui.insert(kLanguageKey, languagePreferenceToken(preference));
    root.insert(kUiSectionKey, ui);
    PreferenceDocument::savePreferencesObject(root);
}

void saveStoredThemePreference(PreferenceDocument::ThemePreference preference)
{
    QJsonObject root = PreferenceDocument::loadPreferencesObject();
    QJsonObject ui = root.value(kUiSectionKey).toObject();
    ui.insert(kThemeKey, PreferenceDocument::themePreferenceToken(preference));
    root.insert(kUiSectionKey, ui);
    PreferenceDocument::savePreferencesObject(root);
}

void saveStoredThemePalette(const char* key, PreferenceDocument::ThemePalette palette)
{
    QJsonObject root = PreferenceDocument::loadPreferencesObject();
    QJsonObject ui = root.value(kUiSectionKey).toObject();
    ui.insert(QLatin1String(key), PreferenceDocument::themePaletteToken(palette));
    root.insert(kUiSectionKey, ui);
    PreferenceDocument::savePreferencesObject(root);
}

PreferenceDocument::LanguagePreference languageListPreference(const QStringList& languages)
{
    for (const QString& language : languages) {
        const QString token = normalizedLanguageToken(language);
        if (token.startsWith("zh")) {
            return PreferenceDocument::LanguagePreference::Chinese;
        }
        if (token.startsWith("ja") || token == "jp") {
            return PreferenceDocument::LanguagePreference::Japanese;
        }
        if (token.startsWith("en")) {
            return PreferenceDocument::LanguagePreference::English;
        }
    }
    return PreferenceDocument::LanguagePreference::System;
}

PreferenceDocument::LanguagePreference resolvedLanguagePreference()
{
    const QByteArray env = qgetenv("MIACODE_LANG").trimmed();
    if (!env.isEmpty()) {
        const PreferenceDocument::LanguagePreference envPreference = parseLanguagePreference(QString::fromUtf8(env));
        if (envPreference != PreferenceDocument::LanguagePreference::System) {
            return envPreference;
        }
    }

    const PreferenceDocument::LanguagePreference storedPreference = PreferenceDocument::preferredLanguage();
    if (storedPreference != PreferenceDocument::LanguagePreference::System) {
        return storedPreference;
    }

    const QStringList uiLanguages = QLocale::system().uiLanguages();
    const PreferenceDocument::LanguagePreference uiLanguagePreference = languageListPreference(uiLanguages);
    if (uiLanguagePreference != PreferenceDocument::LanguagePreference::System) {
        return uiLanguagePreference;
    }

    const QString localeName = normalizedLanguageToken(QLocale::system().name());
    if (localeName.startsWith("zh")) {
        return PreferenceDocument::LanguagePreference::Chinese;
    }
    if (localeName.startsWith("ja") || localeName == "jp") {
        return PreferenceDocument::LanguagePreference::Japanese;
    }
    return PreferenceDocument::LanguagePreference::English;
}

bool isBuiltInLanguageToken(const QString& raw)
{
    const QString token = normalizedLanguageToken(raw);
    return token == "system" || token == "en" || token == "en_us" || token == "en_gb"
        || token == "zh" || token == "zh_cn" || token == "zh_hans" || token == "zh_hans_cn" || token == "cn"
        || token == "ja" || token == "ja_jp" || token == "jp";
}

QString builtInLanguageToken(const QString& raw)
{
    const QString token = normalizedLanguageToken(raw);
    if (token == "en_us" || token == "en_gb") {
        return QStringLiteral("en");
    }
    if (token == "zh_cn" || token == "zh_hans" || token == "zh_hans_cn" || token == "cn") {
        return QStringLiteral("zh");
    }
    if (token == "ja_jp" || token == "jp") {
        return QStringLiteral("ja");
    }
    if (token == "en" || token == "zh" || token == "ja") {
        return token;
    }
    return QStringLiteral("system");
}

QString resolvedLanguageTokenFromStorage()
{
    const QByteArray env = qgetenv("MIACODE_LANG").trimmed();
    if (!env.isEmpty()) {
        const QString envToken = normalizedLanguageToken(QString::fromUtf8(env));
        if (isBuiltInLanguageToken(envToken)) {
            return builtInLanguageToken(envToken);
        }
    }

    const QString storedToken = normalizedLanguageToken(
        PreferenceDocument::loadPreferencesObject().value(QString::fromLatin1(kUiSectionKey)).toObject()
            .value(QString::fromLatin1(kLanguageKey)).toString(QStringLiteral("system")));
    if (isBuiltInLanguageToken(storedToken)) {
        const QString normalized = builtInLanguageToken(storedToken);
        if (normalized != QStringLiteral("system")) {
            return normalized;
        }
    }

    const PreferenceDocument::LanguagePreference resolved = resolvedLanguagePreference();
    switch (resolved) {
    case PreferenceDocument::LanguagePreference::Chinese:
        return QStringLiteral("zh");
    case PreferenceDocument::LanguagePreference::Japanese:
        return QStringLiteral("ja");
    case PreferenceDocument::LanguagePreference::English:
    case PreferenceDocument::LanguagePreference::System:
    default:
        return QStringLiteral("en");
    }
}

PreferenceDocument::Repository& preferencesRepository()
{
    static PreferenceDocument::Repository repository(preferencesPath(), migrationSourcePaths());
    return repository;
}
}  // namespace

QString PreferenceDocument::themePreferenceToken(ThemePreference preference)
{
    switch (preference) {
    case ThemePreference::Light:
        return QStringLiteral("light");
    case ThemePreference::Dark:
        return QStringLiteral("dark");
    case ThemePreference::Legacy:
        return QStringLiteral("legacy");
    case ThemePreference::System:
    default:
        return QStringLiteral("system");
    }
}

PreferenceDocument::ThemePreference PreferenceDocument::themePreferenceFromToken(const QString& token)
{
    const QString raw = token.trimmed().toLower();
    if (raw == QLatin1String("light")) {
        return ThemePreference::Light;
    }
    if (raw == QLatin1String("dark")) {
        return ThemePreference::Dark;
    }
    if (raw == QLatin1String("legacy")) {
        return ThemePreference::Legacy;
    }
    return ThemePreference::System;
}

QString PreferenceDocument::themePaletteToken(ThemePalette palette)
{
    switch (palette) {
    case ThemePalette::Light:
        return QStringLiteral("light");
    case ThemePalette::Dark:
        return QStringLiteral("dark");
    case ThemePalette::Legacy:
        return QStringLiteral("legacy");
    case ThemePalette::LegacyLight:
        return QStringLiteral("legacy_light");
    default:
        return QStringLiteral("dark");
    }
}

PreferenceDocument::ThemePalette PreferenceDocument::themePaletteFromToken(const QString& token)
{
    const QString normalized = token.trimmed().toLower();
    if (normalized == QLatin1String("light")) {
        return ThemePalette::Light;
    }
    if (normalized == QLatin1String("legacy")) {
        return ThemePalette::Legacy;
    }
    if (normalized == QLatin1String("legacy_light")) {
        return ThemePalette::LegacyLight;
    }
    return ThemePalette::Dark;
}

bool PreferenceDocument::themePaletteIsDark(ThemePalette palette)
{
    return palette == ThemePalette::Dark || palette == ThemePalette::Legacy;
}

PreferenceDocument::LanguagePreference PreferenceDocument::preferredLanguage()
{
    return parseLanguagePreference(preferredLanguageToken());
}

void PreferenceDocument::setPreferredLanguage(LanguagePreference preference)
{
    saveStoredLanguagePreference(preference);
}
QString PreferenceDocument::preferredLanguageToken()
{
    const QJsonObject root = loadPreferencesObject();
    return normalizedLanguageToken(
        root.value(QString::fromLatin1(kUiSectionKey)).toObject()
            .value(QString::fromLatin1(kLanguageKey)).toString(QStringLiteral("system")));
}

void PreferenceDocument::setPreferredLanguageToken(const QString& token)
{
    const QString normalized = normalizedLanguageToken(token);
    QJsonObject root = loadPreferencesObject();
    QJsonObject ui = root.value(QString::fromLatin1(kUiSectionKey)).toObject();
    ui.insert(QString::fromLatin1(kLanguageKey), normalized.isEmpty() ? QStringLiteral("system") : normalized);
    root.insert(QString::fromLatin1(kUiSectionKey), ui);
    savePreferencesObject(root);
}

QString PreferenceDocument::resolvedLanguageToken()
{
    return resolvedLanguageTokenFromStorage();
}

QVector<PreferenceDocument::LanguageOption> PreferenceDocument::availableLanguageOptions()
{
    return {
        {QStringLiteral("system"), qtTrId("dialog.preferences.language.system"), true},
        {QStringLiteral("en"), qtTrId("dialog.preferences.language.english"), true},
        {QStringLiteral("zh"), qtTrId("dialog.preferences.language.chinese"), true},
        {QStringLiteral("ja"), qtTrId("dialog.preferences.language.japanese"), true},
    };
}

bool PreferenceDocument::isLanguageAvailable(const QString& token)
{
    const QString normalized = normalizedLanguageToken(token);
    if (isBuiltInLanguageToken(normalized)) {
        return true;
    }
    return false;
}

bool PreferenceDocument::ensurePreferredLanguageAvailable()
{
    const QString token = preferredLanguageToken();
    if (token.isEmpty() || isLanguageAvailable(token)) {
        return false;
    }
    setPreferredLanguageToken(QStringLiteral("system"));
    return true;
}

PreferenceDocument::ThemePreference PreferenceDocument::preferredTheme()
{
    return themePreferenceFromToken(themeTokenFromPreferencesObject(loadPreferencesObject()));
}

void PreferenceDocument::setPreferredTheme(ThemePreference preference)
{
    saveStoredThemePreference(preference);
}


PreferenceDocument::ThemePalette PreferenceDocument::preferredLightTheme()
{
    return loadStoredThemePalette(kLightThemeKey);
}

void PreferenceDocument::setPreferredLightTheme(ThemePalette palette)
{
    saveStoredThemePalette(kLightThemeKey, palette);
}

PreferenceDocument::ThemePalette PreferenceDocument::preferredDarkTheme()
{
    return loadStoredThemePalette(kDarkThemeKey);
}

void PreferenceDocument::setPreferredDarkTheme(ThemePalette palette)
{
    saveStoredThemePalette(kDarkThemeKey, palette);
}

PreferenceDocument::LanguagePreference PreferenceDocument::resolvedLanguage()
{
    return resolvedLanguagePreference();
}


QString PreferenceDocument::preferencesFilePath()
{
    return preferencesPath();
}

QString PreferenceDocument::currentPreferencesSchema()
{
    return QString::fromLatin1(kPreferencesSchema);
}

PreferenceDocument::Repository::Repository(QString path, QStringList migrationSources)
    : path_(std::move(path)), migrationSources_(std::move(migrationSources))
{}

void PreferenceDocument::Repository::initializeLocked()
{
    if (initialized_) {
        return;
    }
    initialized_ = true;
    using namespace miacode::preference_json_file;
    const ReadResult primary = read(path_);
    ReadResult source = primary;
    QString sourcePath = path_;
    // Sources are imported only while the primary is missing; a corrupt or
    // unreadable primary is recovered in place and never re-imported.
    if (primary.status == ReadStatus::Missing) {
        for (const QString& candidate : std::as_const(migrationSources_)) {
            if (candidate.isEmpty()) continue;
            source = read(candidate);
            sourcePath = candidate;
            if (source.status != ReadStatus::Missing) break;
        }
        if (source.status == ReadStatus::Valid && !supportedPreferencesSchema(source.object)) {
            // A source owned by another release is never written here, so an
            // unknown version only skips the import instead of blocking saves.
            miacode::preference_json_file::log(sourcePath,
                QStringLiteral("unsupported-schema migration-skipped"), miacode::debug_log::Level::Warn);
            source = {};
        }
    }
    sourceReadBlocked_ = source.status == ReadStatus::ReadError;
    unreadSourcePath_ = sourcePath;
    unsupportedSchema_ = !supportedPreferencesSchema(source.object);
    if (unsupportedSchema_) {
        miacode::preference_json_file::log(path_,
            QStringLiteral("unsupported-schema read-only-for-session"), miacode::debug_log::Level::Warn);
    }
    if (primary.status == ReadStatus::Missing && source.status == ReadStatus::Valid) {
        migrationStatus_ = QStringLiteral("pending");
        miacode::preference_json_file::log(path_,
            QStringLiteral("imported-from=%1").arg(sourcePath), miacode::debug_log::Level::Info);
    }
    storedSchema_ = source.object.value(QStringLiteral("schema")).toString();
    root_ = normalizedPreferencesRoot(source.object);
    dirty_ = primary.status != ReadStatus::Valid || root_ != primary.object;
    // First-run/legacy migration and corruption recovery share the guarded
    // writer. Read errors remain pending and never cause a blind replacement.
    if (!sourceReadBlocked_
        && (primary.status == ReadStatus::Missing || primary.status == ReadStatus::Corrupt)) {
        flushLocked();
    }
}

QJsonObject PreferenceDocument::Repository::snapshot()
{
    const QMutexLocker lock(&mutex_);
    initializeLocked();
    return root_;
}

QString PreferenceDocument::Repository::storedSchema()
{
    const QMutexLocker lock(&mutex_);
    initializeLocked();
    return storedSchema_;
}

QString PreferenceDocument::Repository::migrationStatus()
{
    const QMutexLocker lock(&mutex_);
    initializeLocked();
    return migrationStatus_;
}

bool PreferenceDocument::Repository::replace(const QJsonObject& root)
{
    const QMutexLocker lock(&mutex_);
    initializeLocked();
    QJsonObject next = root;
    if (unsupportedSchema_) {
        // Runtime edits remain effective, but callers cannot relabel an unknown
        // document and thereby bypass this repository lifetime's write guard.
        next.insert(QStringLiteral("schema"), root_.value(QStringLiteral("schema")));
    } else {
        unsupportedSchema_ = !supportedPreferencesSchema(root);
        next = normalizedPreferencesRoot(root);
    }
    dirty_ = dirty_ || root_ != next;
    root_ = next;
    return flushLocked();
}

bool PreferenceDocument::Repository::flush()
{
    const QMutexLocker lock(&mutex_);
    initializeLocked();
    return flushLocked();
}

bool PreferenceDocument::Repository::flushLocked()
{
    if (unsupportedSchema_) {
        miacode::preference_json_file::log(path_, QStringLiteral("unsupported-schema save-blocked-for-session"));
        return false;
    }
    if (!dirty_) {
        return true;
    }
    if (sourceReadBlocked_) {
        // IO recovery cannot make the fallback snapshot a complete copy of an
        // unread source. A new repository must load it before writes resume.
        miacode::preference_json_file::log(unreadSourcePath_,
            QStringLiteral("source-read-unavailable save-blocked-for-session"));
        return false;
    }
    if (!miacode::preference_json_file::write(path_, root_)) {
        return false;
    }
    dirty_ = false;
    if (migrationStatus_ == QLatin1String("pending")) {
        migrationStatus_ = QStringLiteral("imported");
    }
    return true;
}

bool PreferenceDocument::Repository::isDirty()
{
    const QMutexLocker lock(&mutex_);
    initializeLocked();
    return dirty_;
}

QString PreferenceDocument::storedPreferencesSchema()
{
    return preferencesRepository().storedSchema();
}

QString PreferenceDocument::migrationStatus()
{
    return preferencesRepository().migrationStatus();
}

QJsonObject PreferenceDocument::loadPreferencesObject()
{
    return preferencesRepository().snapshot();
}

bool PreferenceDocument::savePreferencesObject(const QJsonObject& root)
{
    return preferencesRepository().replace(root);
}

QJsonObject PreferenceDocument::normalizePreferencesObject(const QJsonObject& root)
{
    return normalizedPreferencesRoot(root);
}

QString PreferenceDocument::themeTokenFromPreferencesObject(const QJsonObject& root)
{
    return root.value(QStringLiteral("ui")).toObject()
        .value(QStringLiteral("theme")).toString(QStringLiteral("system"));
}

void PreferenceDocument::setThemeTokenInPreferencesObject(QJsonObject* root, const QString& token)
{
    if (root == nullptr) {
        return;
    }
    QJsonObject ui = root->value(QStringLiteral("ui")).toObject();
    ui.insert(QStringLiteral("theme"), token);
    root->insert(QStringLiteral("ui"), ui);
    root->remove(QStringLiteral("theme"));
}
