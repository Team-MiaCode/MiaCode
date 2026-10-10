#include "media_tools/net/NetHttpTransport.h"

#include <QElapsedTimer>
#include <QFile>
#include <QHttpMultiPart>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QSaveFile>
#include <QTimer>

#include <memory>

namespace miacode::net {
namespace {
QJsonObject error(const QString& code)
{
    return {{QStringLiteral("code"), code}, {QStringLiteral("message"), code},
            {QStringLiteral("retryable"), code == QLatin1String("upstream.network")
                || code == QLatin1String("upstream.timeout")}};
}

class Transfer final : public QObject {
public:
    Transfer(const NetHttpRequest& value, NetTransportPort::Done callback,
             NetTransportPort::Progress progressCallback, QObject* parent)
        : QObject(parent), request(value), done(std::move(callback)), progress(std::move(progressCallback))
    {
        timer.setSingleShot(true);
        elapsed.start();
    }
    NetHttpRequest request;
    NetTransportPort::Done done;
    NetTransportPort::Progress progress;
    QPointer<QNetworkReply> reply;
    QTimer timer;
    QElapsedTimer elapsed;
    std::unique_ptr<QSaveFile> file;
    NetHttpResponse response;
    bool timedOut = false;

    void fail(const QString& code)
    {
        if (response.error.isEmpty()) response.error = error(code);
        if (reply) reply->abort();
    }

    void consume()
    {
        if (!reply || !response.error.isEmpty() || response.cancelled) return;
        while (reply->bytesAvailable() > 0) {
            const QByteArray chunk = reply->read(64 * 1024);
            if (response.bytes > request.maximumBytes - chunk.size()) {
                fail(QStringLiteral("upstream.payload_too_large"));
                return;
            }
            if (file) {
                if (response.payload.size() < 4096) response.payload.append(chunk.left(4096 - response.payload.size()));
                if (file->write(chunk) != chunk.size()) {
                    fail(QStringLiteral("file.write_failed"));
                    return;
                }
            } else {
                response.payload.append(chunk);
            }
            response.bytes += chunk.size();
            if (progress && request.method == "GET") {
                bool validLength = false;
                const auto length = reply->rawHeader("Content-Length").toLongLong(&validLength);
                progress(response.bytes, validLength && reply->rawHeader("Content-Encoding").isEmpty() ? length : -1);
            }
        }
    }

    void complete()
    {
        if (!done) return;
        timer.stop();
        if (reply) {
            consume();
            response.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (response.status > 0) response.sent = true;
            response.contentType = reply->rawHeader("Content-Type");
            response.retryAfter = reply->rawHeader("Retry-After");
            const QByteArray lengthHeader = reply->rawHeader("Content-Length");
            bool hasLength = false;
            const qint64 length = lengthHeader.toLongLong(&hasLength);
            response.declaredEmpty = hasLength && length == 0 && response.bytes == 0;
            const QByteArray probe = response.payload.left(4096).toLower();
            const bool html = response.contentType.toLower().contains("text/html")
                || probe.trimmed().startsWith("<!doctype html") || probe.trimmed().startsWith("<html");
            const bool challenge = html && (probe.contains("cloudflare") || probe.contains("challenge")
                || probe.contains("cf-ray") || probe.contains("attention required"));
            if (response.cancelled) response.error = error(QStringLiteral("job.cancelled"));
            else if (timedOut) response.error = error(QStringLiteral("upstream.timeout"));
            else if (response.error.isEmpty()) {
                if (response.status == 413) {
                    response.error = error(QStringLiteral("upstream.payload_too_large"));
                } else if (response.status == 422) {
                    response.error = error(QStringLiteral("upstream.validation"));
                } else if (response.status == 401) {
                    response.error = error(QStringLiteral("upstream.auth_required"));
                } else if (response.status == 429 || (html && probe.contains("1015"))) {
                    response.error = error(QStringLiteral("upstream.rate_limited"));
                } else if (response.status == 403 || challenge) {
                    response.error = error(QStringLiteral("upstream.blocked"));
                } else if (response.status < 200 || response.status >= 300) {
                    response.error = error(response.status == 0 ? QStringLiteral("upstream.network")
                        : QStringLiteral("upstream.http_error"));
                } else if (reply->error() != QNetworkReply::NoError) {
                    response.error = error(QStringLiteral("upstream.network"));
                } else if (file && html) {
                    response.error = error(QStringLiteral("upstream.invalid_payload"));
                } else if (request.requireNonEmpty && response.bytes == 0
                    && !(request.allowDeclaredEmptyChart && response.declaredEmpty)) {
                    response.error = error(QStringLiteral("resource.incomplete"));
                } else if (hasLength && reply->rawHeader("Content-Encoding").isEmpty()
                    && length != response.bytes) {
                    response.error = error(QStringLiteral("resource.incomplete"));
                }
            }
            if (response.status > 0 && !response.error.isEmpty()) response.error.insert(QStringLiteral("upstreamStatus"), response.status);
            reply->deleteLater();
        }
        if (file) {
            if (!response.error.isEmpty()) file->cancelWriting();
            else if (!file->commit()) response.error = error(QStringLiteral("file.write_failed"));
        }
        response.elapsedMs = elapsed.elapsed();
        if (!reply && response.cancelled) response.error = error(QStringLiteral("job.cancelled"));
        auto callback = std::move(done);
        QPointer<Transfer> guard(this);
        callback(std::move(response));
        if (guard) guard->deleteLater();
    }
};
}

NetHttpTransport::NetHttpTransport(QObject* parent)
    : QObject(parent), proxy_(QNetworkProxy::DefaultProxy) {}

NetHttpTransport::~NetHttpTransport()
{
    // Stop replies before destroying managers; completion remains on their owning thread.
    const auto transfers = findChildren<QObject*>(QString(), Qt::FindDirectChildrenOnly);
    for (auto* child : transfers) {
        auto* transfer = dynamic_cast<Transfer*>(child);
        if (!transfer) continue;
        transfer->done = {};
        if (transfer->reply) {
            QObject::disconnect(transfer->reply, nullptr, transfer, nullptr);
            transfer->reply->abort();
        }
    }
}

QNetworkAccessManager* NetHttpTransport::manager(const QString& accountRef)
{
    auto* result = managers_.value(accountRef);
    if (!result) {
        result = new QNetworkAccessManager(this);
        result->setProxy(proxy_);
        managers_.insert(accountRef, result);
    }
    return result;
}

NetTransportPort::Cancel NetHttpTransport::request(const NetHttpRequest& value, Done done, Progress progress)
{
    auto* transfer = new Transfer(value, std::move(done), std::move(progress), this);
    QPointer<Transfer> weak(transfer);
    const auto reject = [transfer](const QString& code) {
        transfer->response.error = error(code);
        QTimer::singleShot(0, transfer, [transfer] { transfer->complete(); });
    };
    if ((value.url.scheme() != QLatin1String("https") && value.url.scheme() != QLatin1String("http"))
        || value.url.host().isEmpty() || !value.url.userInfo().isEmpty()
        || (value.method != "GET" && value.method != "POST")
        || value.maximumBytes <= 0 || value.timeoutMs < 1 || value.timeoutMs > 90000) {
        reject(QStringLiteral("request.invalid"));
    } else {
        if (!value.outputPath.isEmpty()) {
            transfer->file = std::make_unique<QSaveFile>(value.outputPath);
            if (!transfer->file->open(QIODevice::WriteOnly)) reject(QStringLiteral("file.write_failed"));
        }
        if (transfer->response.error.isEmpty()) {
            QNetworkRequest request(value.url);
            request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
            request.setRawHeader("User-Agent", "MiaCode/2 Net");
            request.setRawHeader("Accept", "*/*");
            for (const auto& header : value.headers) request.setRawHeader(header.first, header.second);
            QHttpMultiPart* multipart = nullptr;
            if (!value.parts.isEmpty()) {
                multipart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
                for (const auto& part : value.parts) {
                    QHttpPart item;
                    QByteArray disposition = "form-data; name=\"" + part.name + '"';
                    if (!part.filePath.isEmpty()) disposition += "; filename=\"" + part.fileName.toUtf8() + '"';
                    item.setHeader(QNetworkRequest::ContentDispositionHeader, disposition);
                    if (!part.contentType.isEmpty()) item.setHeader(QNetworkRequest::ContentTypeHeader, part.contentType);
                    if (part.filePath.isEmpty()) item.setBody(part.value);
                    else {
                        auto* file = new QFile(part.filePath, multipart);
                        if (!file->open(QIODevice::ReadOnly)) {
                            reject(QStringLiteral("file.denied"));
                            break;
                        }
                        item.setBodyDevice(file);
                    }
                    multipart->append(item);
                }
            }
            if (transfer->response.error.isEmpty()) {
                auto* network = manager(value.accountRef);
                transfer->reply = value.method == "GET" ? network->get(request)
                    : multipart ? network->post(request, multipart) : network->post(request, value.body);
                if (multipart) multipart->setParent(transfer->reply);
                connect(transfer->reply, &QNetworkReply::readyRead, transfer, [transfer] { transfer->consume(); });
                if (value.method == "POST") connect(transfer->reply, &QNetworkReply::uploadProgress, transfer,
                    [transfer](qint64 sent, qint64 total) {
                        if (sent > 0) transfer->response.sent = true;
                        if (transfer->progress) transfer->progress(sent, total);
                    });
                connect(transfer->reply, &QNetworkReply::finished, transfer, [transfer] { transfer->complete(); });
                connect(&transfer->timer, &QTimer::timeout, transfer, [transfer] {
                    transfer->timedOut = true;
                    if (transfer->reply) transfer->reply->abort();
                });
                transfer->timer.start(value.timeoutMs);
            } else delete multipart;
        }
    }
    return [weak] {
        if (!weak || !weak->done) return;
        weak->response.cancelled = true;
        if (weak->reply) weak->reply->abort();
    };
}

void NetHttpTransport::releaseAccount(const QString& accountRef)
{
    auto* network = managers_.take(accountRef);
    if (!network) return;
    // Active callers release only after their account's requests have completed.
    network->deleteLater();
}

void NetHttpTransport::setProxy(const QNetworkProxy& proxy)
{
    proxy_ = proxy;
    for (auto* network : managers_) network->setProxy(proxy);
}

} // namespace miacode::net
