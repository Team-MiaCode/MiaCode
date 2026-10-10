#pragma once
#include <QString>
#include <QPair>
#include <QJsonObject>

namespace miacode {
class NetAccountStore final {
public:
    static bool available();
    static QPair<QString, QString> load(const QString& principal);
    static bool save(const QString& principal, const QString& username, const QString& password);
    static void erase(const QString& principal);
    static QString migrateLegacyCredentials(const QString& principal, QJsonObject& app);
};
}
