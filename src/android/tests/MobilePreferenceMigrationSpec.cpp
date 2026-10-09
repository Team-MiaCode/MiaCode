#include "app/ui/preferences/PreferenceDocument.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QDebug>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;
    const QString legacyPath = directory.filePath("legacy.json");
    const QString currentPath = directory.filePath("current.json");
    PreferenceDocument::setPreferencesFilePath(currentPath);
    auto write = [](const QString& path, const QByteArray& data) {
        QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
    };
    auto bytes = [](const QString& path) { QFile file(path); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray(); };
    bool ok = true;
    auto require = [&](bool value, const char* message) { if (!value) { qCritical() << message; ok = false; } };
    QString error;
    const QString identity = "android_config_preferences_v1";
    require(PreferenceDocument::migrateFromFile(legacyPath, identity, &error) && !QFile::exists(currentPath),
        "new installation with no earlier file does not manufacture migration state");
    const QJsonObject legacy{{"ui", QJsonObject{{"theme", "light"}, {"editor_text_font_size", 18}}},
        {"app", QJsonObject{{"cover_export", QJsonObject{{"presets", QJsonArray{QJsonObject{{"name", "User cover"}}}},
            {"output", "private/cover-output"}}}, {"video_export", QJsonObject{{"fps", 60}}}}},
        {"future", QJsonObject{{"explicitNull", 9}, {"fromEarlierVersion", true}}}};
    const auto legacyBytes = QJsonDocument(legacy).toJson();
    require(write(legacyPath, legacyBytes), "create earlier settings fixture");
    require(PreferenceDocument::savePreferencesObject({{"ui", QJsonObject{{"theme", "dark"}}},
        {"app", QJsonObject{{"video_export", QJsonObject{{"fps", 30}}}}},
        {"future", QJsonObject{{"explicitNull", QJsonValue(QJsonValue::Null)}}}}), "create current settings fixture");
    require(PreferenceDocument::migrateFromFile(legacyPath, identity, &error), "import earlier preferences");
    auto current = PreferenceDocument::loadPreferencesObject();
    require(current.value("ui").toObject().value("theme") == "dark"
        && current.value("ui").toObject().value("editor_text_font_size") == 18,
        "current choices win and absent nested editor settings survive");
    auto application = current.value("app").toObject();
    require(application.value("cover_export").toObject().value("presets").toArray().first().toObject().value("name") == "User cover"
        && application.value("video_export").toObject().value("fps") == 30,
        "cover presets and video settings move through their real shared document");
    require(current.value("future").toObject().value("explicitNull").isNull()
        && current.value("future").toObject().value("fromEarlierVersion").toBool(), "explicit null and unknown keys preserved");
    require(bytes(legacyPath) == legacyBytes, "earlier settings are never modified");
    auto cover = application.value("cover_export").toObject();
    cover.insert("presets", QJsonArray()); cover.remove("output");
    application.insert("cover_export", cover); current.insert("app", application);
    require(PreferenceDocument::savePreferencesObject(current), "persist deliberate deletion after migration");
    const auto deletedBytes = bytes(currentPath);
    require(PreferenceDocument::migrateFromFile(legacyPath, identity, &error) && bytes(currentPath) == deletedBytes,
        "subsequent starts do not resurrect a deleted preset or output folder");
    PreferenceDocument::setPreferencesFilePath(directory.filePath("first.json"));
    require(PreferenceDocument::migrateFromFile(legacyPath, identity, &error)
        && PreferenceDocument::loadPreferencesObject().value("app").toObject() == legacy.value("app").toObject(),
        "first destination imports complete existing application settings");
    PreferenceDocument::setPreferencesFilePath(currentPath);
    require(write(currentPath, "{corrupt"), "create corrupt current fixture");
    require(!PreferenceDocument::migrateFromFile(legacyPath, identity, &error) && bytes(currentPath) == "{corrupt"
        && !error.isEmpty(), "invalid current document is retained with an error");
    require(write(currentPath, "{}") && write(legacyPath, "[1,2]"), "create invalid earlier fixture");
    require(!PreferenceDocument::migrateFromFile(legacyPath, identity, &error) && bytes(currentPath) == "{}"
        && bytes(legacyPath) == "[1,2]", "invalid earlier document never replaces current settings");
    return ok ? 0 : 1;
}
