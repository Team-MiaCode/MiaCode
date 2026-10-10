#include "app/services/net/NetAccountStore.h"
#include <QCryptographicHash>
#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincred.h>
#endif

namespace miacode {
namespace {
QString target(const QString& principal) {
    return QStringLiteral("MiaCode/Majdata/") + QString::fromLatin1(
        QCryptographicHash::hash(principal.toUtf8(), QCryptographicHash::Sha256).toHex());
}
}
bool NetAccountStore::available() {
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}
QPair<QString, QString> NetAccountStore::load(const QString& principal) {
#ifdef Q_OS_WIN
    const auto key = target(principal);
    PCREDENTIALW credential = nullptr;
    if (!CredReadW(reinterpret_cast<LPCWSTR>(key.utf16()), CRED_TYPE_GENERIC, 0, &credential)) return {};
    const QString username = QString::fromWCharArray(credential->UserName);
    const QString password = QString::fromUtf8(reinterpret_cast<const char*>(credential->CredentialBlob), credential->CredentialBlobSize);
    SecureZeroMemory(credential->CredentialBlob, credential->CredentialBlobSize);
    CredFree(credential);
    return {username, password};
#else
    Q_UNUSED(principal);
    return {};
#endif
}
bool NetAccountStore::save(const QString& principal, const QString& username, const QString& password) {
#ifdef Q_OS_WIN
    auto bytes = password.toUtf8();
    if (bytes.size() > CRED_MAX_CREDENTIAL_BLOB_SIZE) return false;
    const auto key = target(principal);
    CREDENTIALW credential{};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = const_cast<LPWSTR>(reinterpret_cast<LPCWSTR>(key.utf16()));
    credential.UserName = const_cast<LPWSTR>(reinterpret_cast<LPCWSTR>(username.utf16()));
    credential.CredentialBlobSize = static_cast<DWORD>(bytes.size());
    credential.CredentialBlob = reinterpret_cast<LPBYTE>(bytes.data());
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    const bool stored = CredWriteW(&credential, 0);
    SecureZeroMemory(bytes.data(), bytes.size());
    return stored;
#else
    Q_UNUSED(principal); Q_UNUSED(username); Q_UNUSED(password);
    return false;
#endif
}
void NetAccountStore::erase(const QString& principal) {
#ifdef Q_OS_WIN
    const auto key = target(principal);
    CredDeleteW(reinterpret_cast<LPCWSTR>(key.utf16()), CRED_TYPE_GENERIC, 0);
#else
    Q_UNUSED(principal);
#endif
}
QString NetAccountStore::migrateLegacyCredentials(const QString& principal, QJsonObject& app) {
    if (!app.contains("net_upload_password")) return {};
    if (app.value("net_upload_remember_credentials").toBool()) {
        const auto username = app.value("net_upload_username").toString();
        const auto password = app.value("net_upload_password").toString();
        if (!save(principal, username, password) || load(principal) != qMakePair(username, password)) {
            return QStringLiteral("credential_store.unavailable");
        }
        app.insert("net_upload_credential_ref", target(principal));
    }
    app.remove("net_upload_password");
    return {};
}
}
