#pragma once

#include "app/services/net/NetService.h"

#include <QElapsedTimer>
#include <QSet>

namespace miacode::api {

// Supplied by a trusted host adapter after authentication, never from request JSON.
struct ApiContext {
    QString principal;
    QSet<QString> scopes;
    QString requestId;
    QString idempotencyKey;
};

struct ApiReply {
    int status = 200;
    QJsonObject envelope;
};

class ApiDispatcher final {
public:
    explicit ApiDispatcher(NetService& net, QString applicationVersion);
    ApiReply dispatch(const ApiContext& context, const QString& operation, const QJsonObject& parameters = {});
    QJsonObject capabilities(const ApiContext& context) const;

private:
    bool authorized(const ApiContext& context, const QJsonObject& operation) const;
    bool available(const QString& operation) const;
    net::NetCallResult invoke(const ApiContext& context, const QString& operation, const QJsonObject& parameters);
    net::NetCallResult page(const ApiContext& context, const QString& operation, const QJsonObject& parameters);
    struct Cursor {
        QString principal;
        QString operation;
        QString jobId;
        QString kind;
        QString state;
        QJsonArray items;
        QJsonValue version;
        int offset = 0;
        qint64 expiresAt = 0;
    };
    NetService& net_;
    QString applicationVersion_;
    QString hostInstanceId_;
    QElapsedTimer clock_;
    QHash<QString, Cursor> cursors_;
};

} // namespace miacode::api
