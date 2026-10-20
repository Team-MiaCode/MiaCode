#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QUrl>

#include <functional>

namespace miacode::net {

struct NetMultipartPart {
    QByteArray name;
    QByteArray value;
    QString filePath;
    QString fileName;
    QByteArray contentType;
};

struct NetHttpRequest {
    QUrl url;
    QByteArray method = QByteArrayLiteral("GET");
    QList<QPair<QByteArray, QByteArray>> headers;
    QList<NetMultipartPart> parts;
    QByteArray body;
    QString accountRef;
    QString outputPath;
    int timeoutMs = 60000;
    qint64 maximumBytes = 16 * 1024 * 1024;
    bool allowDeclaredEmptyChart = false;
    bool requireNonEmpty = true;
};

struct NetHttpResponse {
    int status = 0;
    QByteArray payload;
    QByteArray contentType;
    QByteArray retryAfter;
    qint64 bytes = 0;
    qint64 elapsedMs = 0;
    QJsonObject error;
    bool cancelled = false;
    bool sent = false;
    bool declaredEmpty = false;
};

class NetTransportPort {
public:
    using Done = std::function<void(NetHttpResponse)>;
    using Progress = std::function<void(qint64 received, qint64 total)>;
    using Cancel = std::function<void()>;
    virtual ~NetTransportPort() = default;
    virtual Cancel request(const NetHttpRequest& request, Done done, Progress progress = {}) = 0;
    virtual void releaseAccount(const QString& accountRef) = 0;
};

} // namespace miacode::net
