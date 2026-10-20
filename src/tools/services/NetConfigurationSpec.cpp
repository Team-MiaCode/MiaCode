#include "app/services/net/NetConfiguration.h"
#include "app/services/PreferenceJsonFile.h"
#include "app/services/PreferenceDocument.h"

#include <QCoreApplication>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QTextStream>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir directory;
    QTextStream err(stderr);
    bool ok = true;
    const auto expect = [&](bool condition, const char* message) {
        if (!condition) { err << "FAIL: " << message << '\n'; ok = false; }
    };
    using miacode::net_configuration::Repository;
    using namespace miacode::preference_json_file;
    const QFileInfo configurationPath(miacode::net_configuration::filePath());
    const QFileInfo preferencesPath(PreferenceDocument::preferencesFilePath());
    expect(configurationPath.fileName() == "net_config.json"
        && configurationPath.dir().absolutePath() == preferencesPath.dir().absolutePath(),
        "Net configuration shares the user preferences directory");
    const QString path = directory.filePath("net_config.json");
    expect(directory.isValid(), "configuration fixture directory");
    miacode::debug_log::setSessionProjectLogDirectory(directory.path());
    Repository missing(path);
    QJsonObject preferences{{"app", QJsonObject{{"last_net_batch_output_dir", "charts"},
        {"language", "zh"}, {"net_upload_remember_credentials", true}}},
        {"ui", QJsonObject{{"theme", "dark"}}}, {"net_upload_username", "user"}};
    const auto original = preferences;
    expect(!missing.enabled() && missing.value("last_net_batch_output_dir").isNull()
        && !missing.value("net_upload_remember_credentials").toBool(), "missing settings use null and false");
    expect(!missing.update({{"net_config", true}}) && !missing.migrate(preferences)
        && preferences == original && !QFileInfo::exists(path), "missing file remains absent and preserves migration source");

    const QJsonObject configured{{"net_config", true}, {"net_upload_username", QJsonValue::Null},
        {"net_upload_remember_credentials", false}, {"custom", 7}};
    expect(write(path, configured), "existing optional configuration");
    Repository enabled(path);
    expect(enabled.enabled(), "explicit true enables Net");
    expect(enabled.migrate(preferences), "legacy settings migrate into existing configuration");
    const auto saved = read(path).object;
    expect(saved.value("last_net_batch_output_dir") == "charts"
        && saved.value("net_upload_username").isNull()
        && !saved.value("net_upload_remember_credentials").toBool()
        && saved.value("custom") == 7, "existing values win including null and false");
    expect(preferences == QJsonObject{{"app", QJsonObject{{"language", "zh"}}},
        {"ui", QJsonObject{{"theme", "dark"}}}}, "migration removes Net settings and preserves other preferences");
    expect(enabled.update({{"last_net_batch_upload_dir", "upload"}})
        && read(path).object.value("last_net_batch_upload_dir") == "upload", "Net updates use their own file");
    preferences = original;
    expect(!enabled.migrate(preferences, [](QJsonObject&) { return false; }) && preferences == original,
        "failed preparation retains the source");
    expect(QFile::remove(path) && !enabled.update({{"net_config", false}})
        && !enabled.migrate(preferences) && preferences == original && !QFileInfo::exists(path),
        "deleted file stays absent on update and migration");

    expect(write(path, {{"net_config", "true"}}), "non-boolean fixture");
    Repository wrongType(path);
    expect(!wrongType.enabled(), "only boolean true enables Net");
    expect(write(path, {{"net_config", false}}), "disabled fixture");
    Repository disabled(path);
    expect(!disabled.enabled(), "false disables Net");
    expect(write(path, {}), "empty fixture");
    Repository empty(path);
    expect(!empty.enabled() && empty.value("net_config").isNull(), "empty file defaults are null and false");

    QFile corrupt(path);
    expect(corrupt.open(QIODevice::WriteOnly) && corrupt.write("{") == 1, "malformed fixture");
    corrupt.close();
    Repository malformed(path);
    expect(!malformed.enabled() && !malformed.update({{"net_config", true}}), "malformed file uses disabled defaults");
    expect(corrupt.open(QIODevice::ReadOnly) && corrupt.readAll() == "{", "malformed source is preserved");
    QTextStream(stdout) << (ok ? "net_configuration_spec ok\n" : "net_configuration_spec failed\n");
    return ok ? 0 : 1;
}
