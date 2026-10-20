#include "app/services/update/PreferenceUpdateStateStore.h"

#include "app/services/PreferenceDocument.h"

#include <QJsonObject>

namespace miacode::update {
namespace {

constexpr auto kGroup = "updates";
constexpr auto kEnabled = "enabled";
constexpr auto kLastCheckAt = "lastCheckAt";
constexpr auto kLastOutcome = "lastOutcome";
constexpr auto kSkippedVersion = "skippedVersion";
constexpr auto kKnownVersion = "knownVersion";

QJsonObject readGroup()
{
    return PreferenceDocument::loadPreferencesObject().value(QLatin1String(kGroup)).toObject();
}

void writeValue(const char* key, const QJsonValue& value)
{
    QJsonObject root = PreferenceDocument::loadPreferencesObject();
    QJsonObject group = root.value(QLatin1String(kGroup)).toObject();
    group.insert(QLatin1String(key), value);
    root.insert(QLatin1String(kGroup), group);
    PreferenceDocument::savePreferencesObject(root);
}

} // namespace

bool PreferenceUpdateStateStore::checkEnabled() const
{
    // 默认开启。用户可以在偏好设置里关掉。
    return readGroup().value(QLatin1String(kEnabled)).toBool(true);
}

void PreferenceUpdateStateStore::setCheckEnabled(bool enabled)
{
    writeValue(kEnabled, enabled);
}

QDateTime PreferenceUpdateStateStore::lastCheckAt() const
{
    const QString text = readGroup().value(QLatin1String(kLastCheckAt)).toString();
    if (text.isEmpty()) {
        return QDateTime();
    }
    return QDateTime::fromString(text, Qt::ISODate);
}

QString PreferenceUpdateStateStore::lastOutcome() const
{
    return readGroup().value(QLatin1String(kLastOutcome)).toString();
}

void PreferenceUpdateStateStore::recordCheck(const QDateTime& atUtc, const QString& outcome)
{
    QJsonObject root = PreferenceDocument::loadPreferencesObject();
    QJsonObject group = root.value(QLatin1String(kGroup)).toObject();
    group.insert(QLatin1String(kLastCheckAt), atUtc.toUTC().toString(Qt::ISODate));
    group.insert(QLatin1String(kLastOutcome), outcome);
    root.insert(QLatin1String(kGroup), group);
    PreferenceDocument::savePreferencesObject(root);
}

QString PreferenceUpdateStateStore::skippedVersion() const
{
    return readGroup().value(QLatin1String(kSkippedVersion)).toString();
}

void PreferenceUpdateStateStore::setSkippedVersion(const QString& version)
{
    writeValue(kSkippedVersion, version);
}

QString PreferenceUpdateStateStore::knownVersion() const
{
    return readGroup().value(QLatin1String(kKnownVersion)).toString();
}

void PreferenceUpdateStateStore::setKnownVersion(const QString& version)
{
    writeValue(kKnownVersion, version);
}

} // namespace miacode::update
