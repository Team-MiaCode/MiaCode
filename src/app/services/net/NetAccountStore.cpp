#include "app/services/net/NetAccountStore.h"
#include "app/services/net/NetConfiguration.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <dpapi.h>
#endif

namespace miacode {
namespace {
QString target(const QString& principal)
{
    return QStringLiteral("MiaCode/Majdata/") + QString::fromLatin1(
        QCryptographicHash::hash(principal.toUtf8(), QCryptographicHash::Sha256).toHex());
}

QString encryptCredentials(const QString& principal, const QString& username, const QString& password)
{
#ifdef Q_OS_WIN
    auto bytes = QJsonDocument(QJsonObject{{"username", username}, {"password", password}})
        .toJson(QJsonDocument::Compact);
    auto entropy = target(principal).toUtf8();
    DATA_BLOB input{static_cast<DWORD>(bytes.size()), reinterpret_cast<BYTE*>(bytes.data())};
    DATA_BLOB context{static_cast<DWORD>(entropy.size()), reinterpret_cast<BYTE*>(entropy.data())};
    DATA_BLOB output{};
    const bool protectedData = CryptProtectData(&input, L"MiaCode Net account", &context,
        nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output);
    SecureZeroMemory(bytes.data(), bytes.size());
    if (!protectedData) return {};
    const auto encrypted = QByteArray(reinterpret_cast<const char*>(output.pbData), output.cbData);
    LocalFree(output.pbData);
    return QString::fromLatin1(encrypted.toBase64());
#else
    Q_UNUSED(principal); Q_UNUSED(username); Q_UNUSED(password);
    return {};
#endif
}

QPair<QString, QString> decryptCredentials(const QString& principal, const QString& encrypted)
{
#ifdef Q_OS_WIN
    auto bytes = QByteArray::fromBase64(encrypted.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    if (bytes.isEmpty()) return {};
    auto entropy = target(principal).toUtf8();
    DATA_BLOB input{static_cast<DWORD>(bytes.size()), reinterpret_cast<BYTE*>(bytes.data())};
    DATA_BLOB context{static_cast<DWORD>(entropy.size()), reinterpret_cast<BYTE*>(entropy.data())};
    DATA_BLOB output{};
    if (!CryptUnprotectData(&input, nullptr, &context, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output))
        return {};
    auto decoded = QByteArray(reinterpret_cast<const char*>(output.pbData), output.cbData);
    SecureZeroMemory(output.pbData, output.cbData);
    LocalFree(output.pbData);
    const auto account = QJsonDocument::fromJson(decoded).object();
    SecureZeroMemory(decoded.data(), decoded.size());
    return {account.value("username").toString(), account.value("password").toString()};
#else
    Q_UNUSED(principal); Q_UNUSED(encrypted);
    return {};
#endif
}
}

bool NetAccountStore::available()
{
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

QPair<QString, QString> NetAccountStore::load(const QString& principal)
{
    const auto accounts = net_configuration::value("net_upload_credentials").toObject();
    return decryptCredentials(principal, accounts.value(target(principal)).toString());
}

bool NetAccountStore::save(const QString& principal, const QString& username, const QString& password)
{
    const QString encrypted = encryptCredentials(principal, username, password);
    if (encrypted.isEmpty()) return false;
    auto accounts = net_configuration::value("net_upload_credentials").toObject();
    accounts.insert(target(principal), encrypted);
    return net_configuration::update({{"net_upload_credentials", accounts}});
}

void NetAccountStore::erase(const QString& principal)
{
    auto accounts = net_configuration::value("net_upload_credentials").toObject();
    if (!accounts.contains(target(principal))) return;
    accounts.remove(target(principal));
    net_configuration::update({{"net_upload_credentials", accounts}});
}
}
