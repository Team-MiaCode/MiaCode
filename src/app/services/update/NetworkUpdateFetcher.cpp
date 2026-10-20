#include "app/services/update/NetworkUpdateFetcher.h"

#include <QNetworkAccessManager>
#include <QNetworkProxyFactory>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace miacode::update {
namespace {

constexpr int kTimeoutMs = 10000;
// manifest 只有几 KB。这个上限防的是被喂一个大文件。
constexpr qint64 kMaxPayloadBytes = 64 * 1024;

} // namespace

NetworkUpdateFetcher::NetworkUpdateFetcher(QObject* parent)
    : QObject(parent)
    , network_(new QNetworkAccessManager(this))
{
    QNetworkProxyFactory::setUseSystemConfiguration(true);
}

NetworkUpdateFetcher::~NetworkUpdateFetcher() = default;

void NetworkUpdateFetcher::fetch(const QUrl& url, Callback callback)
{
    QNetworkRequest request(url);
    request.setTransferTimeout(kTimeoutMs);
    // OSS 前面可能挂 CDN 或换域名，跟随不降级的重定向。
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QVariant::fromValue(QNetworkRequest::NoLessSafeRedirectPolicy));
    request.setMaximumRedirectsAllowed(5);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                         QVariant::fromValue(QNetworkRequest::AlwaysNetwork));

    QNetworkReply* reply = network_->get(request);
    QObject::connect(reply, &QNetworkReply::downloadProgress, reply,
                     [reply](qint64 received, qint64 /*total*/) {
                         if (received > kMaxPayloadBytes) {
                             reply->abort();
                         }
                     });
    QObject::connect(reply, &QNetworkReply::finished, this, [reply, callback]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            callback(false, QByteArray(), reply->errorString());
            return;
        }
        const int status =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status != 200) {
            callback(false, QByteArray(), QStringLiteral("HTTP %1").arg(status));
            return;
        }
        const QByteArray payload = reply->readAll();
        if (payload.size() > kMaxPayloadBytes) {
            callback(false, QByteArray(), QStringLiteral("manifest exceeds the size limit"));
            return;
        }
        callback(true, payload, QString());
    });
}

} // namespace miacode::update
