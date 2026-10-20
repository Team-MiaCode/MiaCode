#pragma once

#include "app/services/update/UpdateFetcher.h"

#include <QObject>

class QNetworkAccessManager;

namespace miacode::update {

// 唯一使用 QNetworkAccessManager 的地方。职责收窄到一次 GET：超时、跟随
// 重定向、响应体上限、走系统代理。请求不带 query 参数、不带自定义 UA、
// 不带 cookie —— 检查更新是纯下行请求，不上报任何本机信息。
class NetworkUpdateFetcher final : public QObject, public UpdateFetcher
{
    Q_OBJECT

public:
    explicit NetworkUpdateFetcher(QObject* parent = nullptr);
    ~NetworkUpdateFetcher() override;

    void fetch(const QUrl& url, Callback callback) override;

private:
    QNetworkAccessManager* network_ = nullptr;
};

} // namespace miacode::update
