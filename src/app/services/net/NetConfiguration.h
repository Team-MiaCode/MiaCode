#pragma once

#include <QJsonObject>
#include <QString>

namespace miacode::net_configuration {

// User Net settings, loaded once per process and persisted on update.
class Repository final {
public:
    explicit Repository(QString path);
    QJsonObject snapshot() const { return settings_; }
    QJsonValue value(const QString& key) const;
    bool update(const QJsonObject& values);
    bool migrate(QJsonObject& preferences);

private:
    bool replace(const QJsonObject& settings);
    QString path_;
    QJsonObject settings_;
    bool available_ = false;
};

QString filePath();
QString configFilePath();
QJsonValue value(const QString& key);
bool enabled();
bool update(const QJsonObject& values);

} // namespace miacode::net_configuration
