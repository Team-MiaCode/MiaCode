#include "media_tools/net/NetHttpTransport.h"
#include "common/DebugLog.h"

#include <QElapsedTimer>
#include <QFile>
#include <QHttpMultiPart>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QSaveFile>
#include <QTimer>
#include <QUuid>

#include <memory>

namespace miacode::net {
namespace {
QString logUrl(const QUrl& url)
{
    return url.toString(QUrl::RemoveUserInfo | QUrl::RemoveQuery | QUrl::RemoveFragment);
}

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
        if (request.method == "GET" && !request.outputPath.isEmpty())
            requestId = QUuid::createUuid().toString(QUuid::Id128);
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
    QString requestId;
    QString stage = QStringLiteral("created");
    qint64 firstDataMs = -1;
    qint64 lastDataMs = -1;
    qint64 progressLogMs = 0;

    void trace(const QString& event, QJsonObject fields = {}) const
    {
        if (requestId.isEmpty()) return;
        fields.insert("event", event);
        fields.insert("request_id", requestId);
        fields.insert("url", logUrl(request.url));
        fields.insert("elapsed_ms", elapsed.elapsed());
        fields.insert("bytes_received", response.bytes);
        fields.insert("stage", stage);
        if (reply) {
            fields.insert("final_url", logUrl(reply->url()));
            fields.insert("http_status", reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
            fields.insert("http2", reply->attribute(QNetworkRequest::Http2WasUsedAttribute).toBool());
        }
        debug_log::appendLine(debug_log::Channel::Runtime, QStringLiteral("net/download/http"),
            QString::fromUtf8(QJsonDocument(fields).toJson(QJsonDocument::Compact)), true,
            event == QLatin1String("timeout") || (event == QLatin1String("complete") && !response.error.isEmpty())
                ? debug_log::Level::Warn : debug_log::Level::Info);
    }

    void markStage(const QString& value)
    {
        stage = value;
        trace(value);
    }

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
            if (!requestId.isEmpty() && !chunk.isEmpty()) {
                lastDataMs = elapsed.elapsed();
                stage = QStringLiteral("receiving");
                if (firstDataMs < 0) {
                    firstDataMs = lastDataMs;
                    trace(QStringLiteral("first_data"));
                }
                if (lastDataMs - progressLogMs >= 5000) {
                    progressLogMs = lastDataMs;
                    trace(QStringLiteral("progress"));
                }
            }
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
        QString fileError;
        if (file) {
            if (file->error() != QFileDevice::NoError) fileError = file->errorString();
            if (!response.error.isEmpty()) file->cancelWriting();
            else if (!file->commit()) {
                response.error = error(QStringLiteral("file.write_failed"));
                fileError = file->errorString();
            }
            // Release the temporary file before callbacks clean up its directory.
            file.reset();
        }
        response.elapsedMs = elapsed.elapsed();
        if (!reply && response.cancelled) response.error = error(QStringLiteral("job.cancelled"));
        if (!requestId.isEmpty()) {
            QJsonObject details{{"error", response.error}, {"timed_out", timedOut}, {"cancelled", response.cancelled},
                {"timeout_ms", request.timeoutMs}, {"first_data_ms", firstDataMs}, {"last_data_ms", lastDataMs},
                {"idle_ms", lastDataMs < 0 ? response.elapsedMs : response.elapsedMs - lastDataMs}, {"file_error", fileError}};
            if (reply) {
                QString networkError = reply->error() == QNetworkReply::NoError ? QString{} : reply->errorString();
                networkError.replace(request.url.toString(), logUrl(request.url));
                networkError.replace(reply->url().toString(), logUrl(reply->url()));
                details.insert("qt_network_error", int(reply->error()));
                details.insert("qt_error_string", networkError);
                details.insert("content_length", QString::fromLatin1(reply->rawHeader("Content-Length")));
                details.insert("content_type", QString::fromLatin1(response.contentType));
                details.insert("content_encoding", QString::fromLatin1(reply->rawHeader("Content-Encoding")));
                details.insert("cf_ray", QString::fromLatin1(reply->rawHeader("CF-Ray")));
                details.insert("retry_after", QString::fromLatin1(response.retryAfter));
            }
            trace(QStringLiteral("complete"), details);
        }
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
    transfer->trace(QStringLiteral("start"), {{"output_path", value.outputPath}, {"timeout_ms", value.timeoutMs},
        {"maximum_bytes", value.maximumBytes}, {"proxy_type", int(proxy_.type())},
        {"proxy_host", proxy_.hostName()}, {"proxy_port", proxy_.port()}});
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
            // Downloads redirect from the API host to the CDN, so GETs follow safe
            // redirects; POSTs stay manual so credentials are never forwarded.
            request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                value.method == QLatin1String("GET") ? QNetworkRequest::NoLessSafeRedirectPolicy
                                                     : QNetworkRequest::ManualRedirectPolicy);
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
                if (!transfer->requestId.isEmpty()) {
                    connect(transfer->reply, &QNetworkReply::socketStartedConnecting, transfer,
                        [transfer] { transfer->markStage(QStringLiteral("socket_connecting")); });
                    connect(transfer->reply, &QNetworkReply::encrypted, transfer,
                        [transfer] { transfer->markStage(QStringLiteral("tls_ready")); });
                    connect(transfer->reply, &QNetworkReply::requestSent, transfer,
                        [transfer] { transfer->markStage(QStringLiteral("request_sent")); });
                    connect(transfer->reply, &QNetworkReply::metaDataChanged, transfer,
                        [transfer] { transfer->markStage(QStringLiteral("response_headers")); });
                    connect(transfer->reply, &QNetworkReply::redirected, transfer, [transfer](const QUrl& url) {
                        transfer->stage = QStringLiteral("redirect");
                        transfer->trace(QStringLiteral("redirect"), {{"redirect_url", logUrl(url)}});
                    });
                }
                connect(transfer->reply, &QNetworkReply::readyRead, transfer, [transfer] { transfer->consume(); });
                if (value.method == "POST") connect(transfer->reply, &QNetworkReply::uploadProgress, transfer,
                    [transfer](qint64 sent, qint64 total) {
                        if (sent > 0) transfer->response.sent = true;
                        if (transfer->progress) transfer->progress(sent, total);
                    });
                connect(transfer->reply, &QNetworkReply::finished, transfer, [transfer] { transfer->complete(); });
                connect(&transfer->timer, &QTimer::timeout, transfer, [transfer] {
                    transfer->timedOut = true;
                    transfer->trace(QStringLiteral("timeout"), {{"timeout_ms", transfer->request.timeoutMs},
                        {"first_data_ms", transfer->firstDataMs}, {"last_data_ms", transfer->lastDataMs},
                        {"idle_ms", transfer->lastDataMs < 0 ? transfer->elapsed.elapsed()
                            : transfer->elapsed.elapsed() - transfer->lastDataMs}});
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
