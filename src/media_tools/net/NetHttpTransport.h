#pragma once

#include "media_tools/net/NetTransportPort.h"

#include <QHash>
#include <QNetworkProxy>
#include <QObject>

class QNetworkAccessManager;

namespace miacode::net {

class NetHttpTransport final : public QObject, public NetTransportPort {
public:
    explicit NetHttpTransport(QObject* parent = nullptr);
    ~NetHttpTransport() override;
    Cancel request(const NetHttpRequest& request, Done done, Progress progress = {}) override;
    void releaseAccount(const QString& accountRef) override;
    void setProxy(const QNetworkProxy& proxy);

private:
    QNetworkAccessManager* manager(const QString& accountRef);
    QHash<QString, QNetworkAccessManager*> managers_;
    QNetworkProxy proxy_;
};

} // namespace miacode::net
