#include "app/services/net/NetConfiguration.h"
#include "app/services/net/NetAccountStore.h"
#include "app/services/PreferenceJsonFile.h"
#include "app/services/PreferenceDocument.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>
#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincred.h>
#endif

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    using miacode::NetAccountStore;
    using miacode::net_configuration::Repository;
    using namespace miacode::preference_json_file;
    const auto arguments = app.arguments();
    if (arguments.contains("--config-probe")) {
        QTextStream(stdout) << (miacode::net_configuration::enabled() ? "enabled" : "disabled");
        return 0;
    }
    if (arguments.size() == 3 && arguments[1] == "--credentials-probe") {
        app.setApplicationName(arguments[2]);
        return NetAccountStore::load("spec") == qMakePair(QStringLiteral("test-user"), QStringLiteral("test-password"))
            ? 0 : 1;
    }

    QTemporaryDir directory;
    app.setApplicationName("MiaCodeNetConfigurationSpec_" + QFileInfo(directory.path()).fileName());
    QTextStream err(stderr);
    bool ok = true;
    const auto expect = [&](bool condition, const char* message) {
        if (!condition) { err << "FAIL: " << message << '\n'; ok = false; }
    };
    expect(directory.isValid(), "configuration fixture directory");
    miacode::debug_log::setSessionProjectLogDirectory(directory.path());
    const QFileInfo configurationPath(miacode::net_configuration::filePath());
    const QFileInfo preferencesPath(PreferenceDocument::preferencesFilePath());
    expect(configurationPath.fileName() == "net_config.json"
        && configurationPath.dir().absolutePath() == preferencesPath.dir().absolutePath(),
        "Net settings share the user preferences directory");
    expect(miacode::net_configuration::configFilePath()
        == QDir(app.applicationDirPath()).filePath("config.json"), "application configuration uses the executable directory");

    const QString probePath = directory.filePath(QFileInfo(app.applicationFilePath()).fileName());
    expect(QFile::copy(app.applicationFilePath(), probePath), "isolated configuration probe");
    const QString configPath = directory.filePath("config.json");
    const auto probe = [&](const QString& expected) {
        QProcess process;
        process.start(probePath, {"--config-probe"});
        return process.waitForFinished(10000) && process.exitStatus() == QProcess::NormalExit
            && process.exitCode() == 0 && QString::fromUtf8(process.readAllStandardOutput()) == expected;
    };
    expect(probe("disabled") && read(configPath).status == ReadStatus::Valid
        && read(configPath).object.isEmpty(), "default application configuration is an empty object");
    expect(write(configPath, {{"net_config", true}}) && probe("enabled"), "user configuration enables upload");
    expect(write(configPath, {{"net_config", false}}) && probe("disabled"), "false closes upload");
    expect(write(configPath, {{"net_config", "true"}}) && probe("disabled"), "upload requires a JSON boolean");
#ifdef Q_OS_WIN
    expect(QDir(directory.path()).mkdir("app")
        && QFile::copy(app.applicationFilePath(), directory.filePath("MiaCode.exe")), "package root fixture");
    const QString packagedProbe = directory.filePath("app/" + QFileInfo(probePath).fileName());
    expect(QFile::copy(app.applicationFilePath(), packagedProbe)
        && write(configPath, {{"net_config", true}}), "packaged application fixture");
    QProcess packaged;
    packaged.start(packagedProbe, {"--config-probe"});
    expect(packaged.waitForFinished(10000) && packaged.exitStatus() == QProcess::NormalExit
        && packaged.exitCode() == 0 && packaged.readAllStandardOutput() == "enabled"
        && !QFileInfo::exists(directory.filePath("app/config.json")), "packaged application reads the root configuration");
#endif

    const QString path = directory.filePath("net_config.json");
    Repository settings(path);
    QJsonObject preferences{{"app", QJsonObject{{"last_net_batch_output_dir", "charts"},
        {"language", "zh"}, {"net_config", true}}}, {"ui", QJsonObject{{"theme", "dark"}}}};
    expect(settings.migrate(preferences) && read(path).object.value("last_net_batch_output_dir") == "charts",
        "Net settings are created and migrated independently of upload configuration");
    expect(preferences == QJsonObject{{"app", QJsonObject{{"language", "zh"}}},
        {"ui", QJsonObject{{"theme", "dark"}}}} && settings.value("net_config").isNull(),
        "migration separates the upload gate from user settings");
    expect(settings.update({{"last_net_batch_upload_dir", "upload"}})
        && read(path).object.value("last_net_batch_upload_dir") == "upload", "Net updates persist their own settings");
    expect(QFile::remove(path) && settings.update({{"last_net_batch_output_dir", "changed"}})
        && read(path).object.value("last_net_batch_output_dir") == "changed", "updates recreate missing user settings");

    expect(write(path, {{"net_config", true}, {"last_net_batch_output_dir", QJsonValue::Null}, {"custom", 7}}),
        "existing settings fixture");
    Repository existing(path);
    QJsonObject legacy{{"last_net_batch_output_dir", "legacy"}};
    expect(existing.migrate(legacy) && existing.value("last_net_batch_output_dir").isNull()
        && existing.value("custom") == 7 && existing.value("net_config").isNull(),
        "explicit values win and the former gate is removed");
    legacy = {{"last_net_batch_output_dir", "legacy"}};
    const auto original = legacy;
    expect(!existing.migrate(legacy, [](QJsonObject&) { return false; }) && legacy == original,
        "failed credential preparation retains migration source");
    QFile corrupt(path);
    expect(corrupt.open(QIODevice::WriteOnly) && corrupt.write("{") == 1, "malformed fixture");
    corrupt.close();
    Repository malformed(path);
    expect(!malformed.update({{"last_net_batch_output_dir", "charts"}}), "malformed settings preserve their source");

#ifdef Q_OS_WIN
    expect(NetAccountStore::save("spec", "test-user", "test-password"), "credentials are encrypted and saved");
    const auto stored = read(miacode::net_configuration::filePath()).object;
    const auto serialized = QJsonDocument(stored).toJson();
    expect(!serialized.contains("test-user") && !serialized.contains("test-password")
        && !stored.value("net_upload_credentials").toObject().isEmpty(), "stored credentials contain ciphertext");
    expect(NetAccountStore::load("spec") == qMakePair(QStringLiteral("test-user"), QStringLiteral("test-password")),
        "encrypted credentials round trip");
    QProcess reader;
    reader.start(app.applicationFilePath(), {"--credentials-probe", app.applicationName()});
    expect(reader.waitForFinished(10000) && reader.exitStatus() == QProcess::NormalExit
        && reader.exitCode() == 0, "credentials survive process restart");
    expect(NetAccountStore::save("other", "other-user", "other-password"), "second principal credentials");
    const auto accountKey = [](const QString& principal) {
        return "MiaCode/Majdata/" + QString::fromLatin1(QCryptographicHash::hash(principal.toUtf8(), QCryptographicHash::Sha256).toHex());
    };
    auto accounts = miacode::net_configuration::value("net_upload_credentials").toObject();
    accounts.insert(accountKey("other"), accounts.value(accountKey("spec")));
    expect(miacode::net_configuration::update({{"net_upload_credentials", accounts}})
        && NetAccountStore::load("other").first.isEmpty(), "ciphertext is bound to its principal");
    auto tampered = QByteArray::fromBase64(accounts.value(accountKey("spec")).toString().toLatin1());
    if (!tampered.isEmpty()) tampered[tampered.size() - 1] = tampered.back() ^ 1;
    accounts.insert(accountKey("spec"), QString::fromLatin1(tampered.toBase64()));
    expect(miacode::net_configuration::update({{"net_upload_credentials", accounts}})
        && NetAccountStore::load("spec").first.isEmpty(), "modified ciphertext fails integrity validation");
    expect(NetAccountStore::save("spec", "test-user", "test-password"), "restore the account fixture");
    accounts = miacode::net_configuration::value("net_upload_credentials").toObject();
    accounts.insert(accountKey("other"), "invalid ciphertext");
    expect(miacode::net_configuration::update({{"net_upload_credentials", accounts}})
        && NetAccountStore::load("other").first.isEmpty(), "invalid ciphertext is rejected");
    NetAccountStore::erase("other");
    expect(NetAccountStore::load("other").first.isEmpty()
        && NetAccountStore::load("spec").first == "test-user", "erasing one account preserves other accounts");
    QJsonObject legacyAccount{{"net_upload_username", "legacy-user"}, {"net_upload_password", "legacy-password"},
        {"net_upload_remember_credentials", true}};
    expect(NetAccountStore::migrateLegacyCredentials("legacy", legacyAccount).isEmpty()
        && !legacyAccount.contains("net_upload_username") && !legacyAccount.contains("net_upload_password")
        && !legacyAccount.value("net_upload_credentials").toObject().isEmpty(), "legacy account becomes encrypted data");
    QJsonObject unreadableAccount{{"net_upload_username", "legacy-user"}, {"net_upload_password", "legacy-password"},
        {"net_upload_remember_credentials", true}, {"net_upload_credentials", QJsonObject{{accountKey("legacy"), "invalid ciphertext"}}}};
    const auto originalAccount = unreadableAccount;
    expect(!NetAccountStore::migrateLegacyCredentials("legacy", unreadableAccount).isEmpty()
        && unreadableAccount == originalAccount, "unreadable destination preserves legacy credentials");

    const QString legacyPrincipal = "legacy_" + app.applicationName();
    const QString legacyTarget = accountKey(legacyPrincipal);
    auto legacyPassword = QByteArrayLiteral("legacy-password");
    CREDENTIALW credential{};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = const_cast<LPWSTR>(reinterpret_cast<LPCWSTR>(legacyTarget.utf16()));
    credential.UserName = const_cast<LPWSTR>(L"legacy-user");
    credential.CredentialBlob = reinterpret_cast<LPBYTE>(legacyPassword.data());
    credential.CredentialBlobSize = static_cast<DWORD>(legacyPassword.size());
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    expect(CredWriteW(&credential, 0), "legacy Windows credential fixture");
    QJsonObject windowsAccount{{"net_upload_username", "legacy-user"}, {"net_upload_remember_credentials", true}};
    expect(NetAccountStore::migrateLegacyCredentials(legacyPrincipal, windowsAccount).isEmpty()
        && miacode::net_configuration::update(windowsAccount)
        && NetAccountStore::load(legacyPrincipal) == qMakePair(QStringLiteral("legacy-user"), QStringLiteral("legacy-password")),
        "Windows credentials migrate to the encrypted settings file");
    NetAccountStore::eraseLegacyCredentials(legacyPrincipal);
    PCREDENTIALW remainingCredential = nullptr;
    const bool remaining = CredReadW(reinterpret_cast<LPCWSTR>(legacyTarget.utf16()), CRED_TYPE_GENERIC, 0, &remainingCredential);
    expect(!remaining && GetLastError() == ERROR_NOT_FOUND, "committed migration releases the legacy Windows credential");
    if (remaining) CredFree(remainingCredential);
    NetAccountStore::erase(legacyPrincipal);
    NetAccountStore::erase("spec");
#endif
    QFile::remove(configurationPath.filePath());
    QFile::remove(preferencesPath.filePath());
    QDir().rmdir(configurationPath.absolutePath());
    QTextStream(stdout) << (ok ? "net_configuration_spec ok\n" : "net_configuration_spec failed\n");
    return ok ? 0 : 1;
}
