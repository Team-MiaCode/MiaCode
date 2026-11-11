#include "app/services/net/NetConfiguration.h"

#include "app/services/PreferenceDocument.h"
#include "app/services/PreferenceJsonFile.h"
#include <QFileInfo>
#include <QDir>

namespace miacode::net_configuration {
namespace {
bool isNetSetting(const QString& key)
{
    return key == QStringLiteral("net") || key.startsWith(QStringLiteral("net_"))
        || key.startsWith(QStringLiteral("net.")) || key.startsWith(QStringLiteral("last_net_"));
}

void extractNetSettings(QJsonObject& preferences, QJsonObject& settings)
{
    const auto keys = preferences.keys();
    for (const auto& key : keys) {
        const auto value = preferences.value(key);
        if (key == QStringLiteral("net_config")) {
            preferences.remove(key);
        } else if (isNetSetting(key)) {
            if (!settings.contains(key)) settings.insert(key, value);
            preferences.remove(key);
        } else if (value.isObject()) {
            auto child = value.toObject();
            extractNetSettings(child, settings);
            preferences.insert(key, child);
        }
    }
}

Repository& repository()
{
    static Repository instance(filePath());
    static const bool initialized = [&] {
        auto preferences = PreferenceDocument::loadPreferencesObject();
        const auto original = preferences;
        return instance.migrate(preferences)
            && (preferences == original || PreferenceDocument::savePreferencesObject(preferences));
    }();
    Q_UNUSED(initialized);
    return instance;
}
}

Repository::Repository(QString path) : path_(std::move(path))
{
    const auto source = preference_json_file::read(path_);
    available_ = source.status == preference_json_file::ReadStatus::Valid
        || source.status == preference_json_file::ReadStatus::Missing;
    settings_ = source.object;
}

QJsonValue Repository::value(const QString& key) const
{
    return settings_.contains(key) ? settings_.value(key) : QJsonValue(QJsonValue::Null);
}

bool Repository::replace(const QJsonObject& settings)
{
    const auto source = preference_json_file::read(path_);
    if (!available_ || (source.status != preference_json_file::ReadStatus::Valid
        && source.status != preference_json_file::ReadStatus::Missing))
        return false;
    if ((settings != settings_ || source.status == preference_json_file::ReadStatus::Missing)
        && !preference_json_file::write(path_, settings)) return false;
    settings_ = settings;
    return true;
}

bool Repository::update(const QJsonObject& values)
{
    auto settings = settings_;
    for (auto it = values.begin(); it != values.end(); ++it) settings.insert(it.key(), it.value());
    return replace(settings);
}

bool Repository::migrate(QJsonObject& preferences)
{
    if (!available_) return false;
    auto remaining = preferences;
    auto settings = settings_;
    extractNetSettings(remaining, settings);
    // The upload gate moved to config.json; legacy plaintext accounts are dropped.
    for (const auto* key : {"net_config", "net_upload_username", "net_upload_password", "net_upload_credential_ref"})
        settings.remove(QLatin1String(key));
    // Commit the destination before releasing the source settings.
    if (!replace(settings)) return false;
    preferences = remaining;
    return true;
}

namespace {
QString userConfigFile(const QString& fileName)
{
    const QString preferences = PreferenceDocument::preferencesFilePath();
    return preferences.isEmpty() ? QString{} : QFileInfo(preferences).dir().filePath(fileName);
}
}

QString filePath() { return userConfigFile(QStringLiteral("net_config.json")); }

QJsonValue value(const QString& key) { return repository().value(key); }

// Hand-edited and never rewritten, so release packages cannot overwrite it.
QString configFilePath() { return userConfigFile(QStringLiteral("config.json")); }

bool enabled()
{
    static const bool uploadEnabled = [] {
        const QString path = configFilePath();
        if (path.isEmpty()) return false;
        const auto source = preference_json_file::read(path);
        if (source.status == preference_json_file::ReadStatus::Missing)
            preference_json_file::write(path, {});
        return source.object.value(QStringLiteral("net_config")).toBool(false);
    }();
    return uploadEnabled;
}

bool update(const QJsonObject& values) { return repository().update(values); }

} // namespace miacode::net_configuration
