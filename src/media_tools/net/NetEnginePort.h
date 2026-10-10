#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>

#include <functional>

namespace miacode::net {

struct NetTaskRequest {
    QString principal;
    QString operation;
    QJsonObject parameters;
    QStringList itemIds;
};

struct NetTaskEvent {
    int itemIndex = -1;
    QString state;
    QString phase;
    QJsonObject error;
    QJsonObject progress;
    QJsonArray artifacts;
};

struct NetTaskResult {
    QJsonValue result = QJsonValue::Null;
    QJsonObject error;
    bool blocked = false;
};

struct NetCallResult {
    QJsonValue result = QJsonValue::Null;
    QJsonObject error;
    bool ok() const { return error.isEmpty(); }
};

class NetEnginePort {
public:
    using Done = std::function<void(NetTaskResult)>;
    using Event = std::function<void(NetTaskEvent)>;
    using Cancel = std::function<void()>;
    virtual ~NetEnginePort() = default;
    virtual bool supports(const QString& operation) const = 0;
    virtual Cancel execute(NetTaskRequest request, Event event, Done done) = 0;
    virtual NetCallResult call(const QString& principal, const QString& operation,
                               const QJsonObject& parameters) = 0;
    // Local host capabilities. Paths stay outside serialized API requests/results.
    virtual QString grantDirectory(const QString&, const QString&) { return {}; }
    virtual QString previewPath(const QString&, const QString&) const { return {}; }
    virtual QString uploadMaterialPath(const QString&, const QString&, const QString&) const { return {}; }
    virtual void setAccountRemembered(const QString&, const QString&, bool) {}
};

} // namespace miacode::net
