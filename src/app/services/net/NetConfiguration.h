#pragma once

#include <QJsonObject>
#include <QString>
#include <functional>

namespace miacode::net_configuration {

// Optional application configuration, loaded once per process.
class Repository final {
public:
    explicit Repository(QString path);
    QJsonObject snapshot() const { return settings_; }
    QJsonValue value(const QString& key) const;
    bool enabled() const;
    bool update(const QJsonObject& values);
    bool migrate(QJsonObject& preferences,
                 const std::function<bool(QJsonObject&)>& prepareSettings = {});

private:
    bool replace(const QJsonObject& settings);
    QString path_;
    QJsonObject settings_;
    bool available_ = false;
};

QString filePath();
QJsonValue value(const QString& key);
bool enabled();
bool update(const QJsonObject& values);

} // namespace miacode::net_configuration
