#include "media_tools/net/NetHttpTransport.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>

namespace {
bool check(bool condition, const char* message)
{
    if (!condition) QTextStream(stderr) << "FAIL: " << message << '\n';
    return condition;
}

class Server {
public:
    Server()
    {
        server.listen(QHostAddress::LocalHost);
        QObject::connect(&server, &QTcpServer::newConnection, &server, [this] {
            while (server.hasPendingConnections()) {
                auto* socket = server.nextPendingConnection();
                QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                QObject::connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
                    auto buffer = socket->property("request").toByteArray() + socket->readAll();
                    socket->setProperty("request", buffer);
                    if (!buffer.contains("\r\n\r\n") || socket->property("responded").toBool()) return;
                    socket->setProperty("responded", true);
                    requests.append(buffer);
                    const QByteArray route = buffer.split(' ').value(1);
                    if (route == "/slow") return;
                    QByteArray status = "200 OK";
                    QByteArray type = "application/json";
                    QByteArray body = "[]";
                    QByteArray extra;
                    if (route == "/empty") body.clear();
                    if (route == "/short") { body = "abc"; extra = "Content-Length: 10\r\n"; }
                    if (route == "/absent") { status = "404 Not Found"; body = "missing"; }
                    if (route == "/challenge") { type = "text/html"; body = "<html>Cloudflare challenge</html>"; }
                    if (route == "/too-large-challenge") { status = "413 Payload Too Large"; type = "text/html"; body = "<html>Cloudflare challenge 1015</html>"; }
                    if (route == "/validation-challenge") { status = "422 Unprocessable Content"; type = "text/html"; body = "<html>Cloudflare challenge</html>"; }
                    if (route == "/cookie") extra = "Set-Cookie: session=example; Path=/\r\n";
                    if (route == "/large") body = QByteArray(2000, 'x');
                    if (route == "/gzip") {
                        type = "application/octet-stream";
                        body = QByteArray::fromHex("1f8b0800000000000203cb48cdc9c95728cf2fca49010085114a0d0b000000");
                        extra = "Content-Encoding: gzip\r\n";
                    }
                    QByteArray headers = "HTTP/1.1 " + status + "\r\nContent-Type: " + type + "\r\n" + extra;
                    if (route != "/short") headers += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
                    headers += "Connection: close\r\n\r\n";
                    socket->write(headers + body);
                    socket->disconnectFromHost();
                });
            }
        });
    }
    QUrl url(const QString& path) const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1%2").arg(server.serverPort()).arg(path));
    }
    QTcpServer server;
    QList<QByteArray> requests;
};

miacode::net::NetHttpResponse run(miacode::net::NetHttpTransport& transport,
                                 const miacode::net::NetHttpRequest& request, bool cancel = false)
{
    QEventLoop loop;
    miacode::net::NetHttpResponse response;
    bool done = false;
    const auto cancellation = transport.request(request, [&](auto value) {
        response = std::move(value);
        done = true;
        loop.quit();
    });
    if (cancel) cancellation();
    if (!done) {
        QTimer::singleShot(3000, &loop, [&] { cancellation(); loop.quit(); });
        loop.exec();
    }
    if (!done) response.error = {{"code", "test.deadline"}};
    return response;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    using namespace miacode::net;
    Server server;
    NetHttpTransport transport;
    transport.setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
    QTemporaryDir directory;
    bool ok = check(server.server.isListening() && directory.isValid(), "local fixture is available");
    NetHttpRequest request;
    request.url = server.url(QStringLiteral("/ok"));
    auto response = run(transport, request);
    ok &= check(response.error.isEmpty() && response.payload == "[]" && response.status == 200,
        "async GET reports actual JSON response");
    request.outputPath = directory.filePath(QStringLiteral("maidata.txt"));
    response = run(transport, request);
    QFile file(request.outputPath);
    ok &= check(file.open(QIODevice::ReadOnly), "committed file opens for verification");
    ok &= check(response.error.isEmpty() && file.readAll() == "[]", "streaming download commits output");
    file.close();
    request.url = server.url(QStringLiteral("/short"));
    response = run(transport, request);
    ok &= check(file.open(QIODevice::ReadOnly), "preserved file opens for verification");
    ok &= check(!response.error.isEmpty() && file.readAll() == "[]", "truncated response preserves existing output");
    file.close();
    request.url = server.url(QStringLiteral("/empty"));
    response = run(transport, request);
    ok &= check(response.error.value("code") == "resource.incomplete", "empty media is rejected");
    request.allowDeclaredEmptyChart = true;
    response = run(transport, request);
    ok &= check(response.error.isEmpty() && response.declaredEmpty && response.bytes == 0,
        "explicitly declared zero-length chart uses compatible policy");
    request.allowDeclaredEmptyChart = false;
    request.url = server.url(QStringLiteral("/challenge"));
    response = run(transport, request);
    ok &= check(response.error.value("code") == "upstream.blocked", "challenge HTML blocks transfer");
    request.url = server.url(QStringLiteral("/too-large-challenge"));
    response = run(transport, request);
    ok &= check(response.error.value("code") == "upstream.payload_too_large", "413 has priority over challenge HTML and rate-limit markers");
    request.url = server.url(QStringLiteral("/validation-challenge"));
    response = run(transport, request);
    ok &= check(response.error.value("code") == "upstream.validation", "422 remains an item validation failure");
    request.url = server.url(QStringLiteral("/absent"));
    response = run(transport, request);
    ok &= check(response.status == 404 && !response.error.isEmpty(), "404 retains status for optional-resource policy");
    request.url = server.url(QStringLiteral("/gzip"));
    response = run(transport, request);
    ok &= check(response.error.isEmpty() && response.bytes == 11,
        "compressed length is not compared against decoded bytes");
    request.url = server.url(QStringLiteral("/large"));
    request.maximumBytes = 100;
    response = run(transport, request);
    ok &= check(response.error.value("code") == "upstream.payload_too_large", "response limit aborts streaming");
    request.outputPath.clear();
    request.maximumBytes = 16000;
    request.url = server.url(QStringLiteral("/slow"));
    request.timeoutMs = 20;
    response = run(transport, request);
    ok &= check(response.error.value("code") == "upstream.timeout", "deadline aborts stalled response");
    request.timeoutMs = 1000;
    response = run(transport, request, true);
    ok &= check(response.cancelled && response.error.value("code") == "job.cancelled", "cancel completes immediately on owner thread");

    request.url = server.url(QStringLiteral("/cookie"));
    request.accountRef = QStringLiteral("account-a");
    run(transport, request);
    request.url = server.url(QStringLiteral("/ok"));
    run(transport, request);
    ok &= check(server.requests.last().contains("session=example"), "account reuses its cookie jar");
    request.accountRef = QStringLiteral("account-b");
    run(transport, request);
    ok &= check(!server.requests.last().contains("session=example"), "separate account does not inherit cookies");
    transport.releaseAccount(QStringLiteral("account-a"));
    request.accountRef = QStringLiteral("account-a");
    run(transport, request);
    ok &= check(!server.requests.last().contains("session=example"), "logout clears account cookies");
    return ok ? 0 : 1;
}
