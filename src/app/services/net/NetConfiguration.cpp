#include "app/services/net/NetConfiguration.h"

#include "app/services/PreferenceDocument.h"
#include "app/services/PreferenceJsonFile.h"
#include "app/services/net/NetAccountStore.h"
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
        if (isNetSetting(key)) {
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
        if (instance.migrate(preferences, [](QJsonObject& settings) {
            const auto error = NetAccountStore::migrateLegacyCredentials(QStringLiteral("desktop"), settings);
            if (!error.isEmpty()) preference_json_file::log(filePath(), error);
            return error.isEmpty();
        }) && preferences != original)
            return PreferenceDocument::savePreferencesObject(preferences);
        return false;
    }();
    Q_UNUSED(initialized);
    return instance;
}
}

Repository::Repository(QString path) : path_(std::move(path))
{
    const auto source = preference_json_file::read(path_);
    available_ = source.status == preference_json_file::ReadStatus::Valid;
    if (available_) settings_ = source.object;
}

QJsonValue Repository::value(const QString& key) const
{
    return settings_.contains(key) ? settings_.value(key) : QJsonValue(QJsonValue::Null);
}

bool Repository::enabled() const
{
    return value(QStringLiteral("net_config")).toBool(false);
}

bool Repository::replace(const QJsonObject& settings)
{
    if (!available_ || preference_json_file::read(path_).status != preference_json_file::ReadStatus::Valid)
        return false;
    if (settings != settings_ && !preference_json_file::write(path_, settings)) return false;
    settings_ = settings;
    return true;
}

bool Repository::update(const QJsonObject& values)
{
    auto settings = settings_;
    for (auto it = values.begin(); it != values.end(); ++it) settings.insert(it.key(), it.value());
    return replace(settings);
}

bool Repository::migrate(QJsonObject& preferences,
                         const std::function<bool(QJsonObject&)>& prepareSettings)
{
    if (!available_) return false;
    auto remaining = preferences;
    auto settings = settings_;
    extractNetSettings(remaining, settings);
    if (prepareSettings && !prepareSettings(settings)) return false;
    // Commit the destination before releasing the source settings.
    if (!replace(settings)) return false;
    preferences = remaining;
    return true;
}

QString filePath()
{
    const QString preferences = PreferenceDocument::preferencesFilePath();
    return preferences.isEmpty() ? QString{}
        : QFileInfo(preferences).dir().filePath(QStringLiteral("net_config.json"));
}

QJsonValue value(const QString& key) { return repository().value(key); }
bool enabled() { return repository().enabled(); }
bool update(const QJsonObject& values) { return repository().update(values); }

} // namespace miacode::net_configuration
