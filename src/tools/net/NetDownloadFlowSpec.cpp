#include "media_tools/net/NetProvider.h"
#include "media_tools/net/NetHttpTransport.h"
#include "media_tools/net/NetClient.h"
#include "app/services/api/ApiDispatcher.h"
#include "app/services/api/ApiCatalog.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QDir>
#include <QDirIterator>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTextStream>
#include <QTimer>
#include <QTranslator>
#include <QJsonDocument>
#include <miniz.h>

using namespace miacode::net;
namespace {
bool check(bool condition, const char* message) { if (!condition) QTextStream(stderr) << "FAIL: " << message << '\n'; return condition; }
class Server : public QTcpServer {
public:
    QStringList paths;
    QHash<QString, int> statuses;
    Server() {
        listen(QHostAddress::LocalHost);
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (auto* socket = nextPendingConnection()) {
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
                    auto request = socket->property("request").toByteArray() + socket->readAll();
                    socket->setProperty("request", request);
                    if (!request.contains("\r\n\r\n") || socket->property("sent").toBool()) return;
                    socket->setProperty("sent", true);
                    const QString path = QString::fromUtf8(request.split(' ').value(1));
                    paths.append(path);
                    const int status = statuses.value(path, 200);
                    const bool empty = path.contains("/empty/chart");
                    const QByteArray body = empty ? QByteArray{} : path.contains("/chart") ? QByteArray("&title=Fixture\n&lv_5=12\n&inote_5=(120){4}1,E\n") : QByteArray("fixture-media");
                    socket->write("HTTP/1.1 " + QByteArray::number(status) + " Response\r\nContent-Type: application/octet-stream\r\nContent-Length: "
                        + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
                    socket->disconnectFromHost();
                });
            }
        });
    }
};
NetTaskResult run(NetProvider& provider, const QString& operation, const QJsonObject& parameters,
                  QStringList ids = {}, NetEnginePort::Event event = {}) {
    QEventLoop loop;
    NetTaskResult result;
    bool finished = false;
    const auto cancel = provider.execute({"desktop", operation, parameters, ids}, event, [&](NetTaskResult value) { result = value; finished = true; loop.quit(); });
    if (!finished) { QTimer::singleShot(10000, &loop, [&] { cancel(); loop.quit(); }); loop.exec(); }
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    if (!finished) result.error = {{"code", "test.timeout"}};
    if (result.error.isEmpty()) {
        const auto schema = miacode::api::ApiCatalog::operation(operation).value("jobResultSchema").toString();
        if (!miacode::api::ApiCatalog::validateResult(schema, result.result)) result.error = {{"code", "internal.contract_violation"}};
    }
    return result;
}
bool zipHas(const QString& path, const char* entry) {
    mz_zip_archive zip{};
    const auto name = path.toUtf8();
    if (!mz_zip_reader_init_file(&zip, name.constData(), 0)) return false;
    const bool result = mz_zip_reader_locate_file(&zip, entry, nullptr, 0) >= 0;
    mz_zip_reader_end(&zip);
    return result;
}
}
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    Server server;
    NetHttpTransport transport;
    NetProvider provider(transport, nullptr, QUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort())));
    QTemporaryDir output(QDir(QStringLiteral(MIACODE_TEST_OUTPUT_ROOT)).filePath("net-download-check-XXXXXX"));
    const auto grant = provider.grantDirectory("desktop", output.path());
    const auto download = [&](const QString& id, bool video, bool zip) {
        return run(provider, "net.downloads.create", {{"providerId", "majdata"}, {"chartIds", QJsonArray{id}},
            {"destination", QJsonObject{{"kind", "directory_grant"}, {"grantRef", grant}}}, {"includeVideo", video}, {"createZip", zip}}, {"item_1"});
    };
    bool ok = check(output.isValid(), "bounded fixture output directory exists");
    QTranslator translator;
    ok &= check(translator.load(QStringLiteral(MIACODE_ZH_CN_QM_PATH)), "compiled Chinese catalog loads");
    app.installTranslator(&translator);
    ok &= check(qtTrId("net.ui.download_page") == QStringLiteral("谱面下载")
        && qtTrId("net.ui.upload_page") == QStringLiteral("谱面上传")
        && qtTrId("net.ui.query") == QStringLiteral("查询"), "formal UI names resolve from compiled translations");

    miacode::JobRegistry jobs;
    miacode::NetService service(jobs, provider);
    miacode::api::ApiDispatcher api(service, "test");
    const miacode::api::ApiContext context{"desktop", {"net.download", "files.write", "jobs.read", "jobs.control"}, {}, "download-fixture"};
    const QJsonObject parameters{{"providerId", "majdata"}, {"chartIds", QJsonArray{"service"}},
        {"destination", QJsonObject{{"kind", "directory_grant"}, {"grantRef", grant}}}, {"includeVideo", false}};
    const auto accepted = api.dispatch(context, "net.downloads.create", parameters);
    const auto jobId = accepted.envelope.value("result").toObject().value("jobId").toString();
    QEventLoop serviceLoop;
    QObject::connect(&jobs, &miacode::JobRegistry::changed, &serviceLoop, [&](const QString& id) {
        if (id == jobId && miacode::JobRegistry::isTerminal(jobs.snapshot(id, "desktop").value("state").toString())) serviceLoop.quit();
    });
    if (!jobId.isEmpty()) {
        QTimer::singleShot(10000, &serviceLoop, [&] { jobs.cancel(jobId, "desktop"); serviceLoop.quit(); });
        serviceLoop.exec();
    }
    const auto repeated = api.dispatch(context, "net.downloads.create", parameters);
    if (accepted.status != 202 || jobs.snapshot(jobId, "desktop").value("state") != "succeeded"
        || repeated.envelope.value("result").toObject().value("jobId") != jobId) {
        QTextStream(stderr) << QJsonDocument(accepted.envelope).toJson(QJsonDocument::Compact) << '\n'
            << QJsonDocument(jobs.snapshot(jobId, "desktop")).toJson(QJsonDocument::Compact) << '\n'
            << QJsonDocument(repeated.envelope).toJson(QJsonDocument::Compact) << '\n';
    }
    ok &= check(accepted.status == 202 && jobs.snapshot(jobId, "desktop").value("state") == "succeeded"
        && repeated.envelope.value("result").toObject().value("jobId") == jobId
        && server.paths.count("/maichart/service/track") == 1, "public download dispatcher validates results and deduplicates idempotency key");
    auto result = download("complete", true, true);
    const auto directory = chartDirectoryPathForTitle(output.path(), "complete", "complete");
    ok &= check(result.error.isEmpty() && result.result.toObject().value("counts").toObject().value("succeeded") == 1
        && QFileInfo::exists(QDir(directory).filePath("pv.mp4")) && zipHas(QDir(directory).filePath("download.zip"), "pv.mp4"),
        "download publishes all resources and ZIP includes PV");
    server.statuses.insert("/maichart/absent/video", 404);
    result = download("absent", true, true);
    const auto absent = chartDirectoryPathForTitle(output.path(), "absent", "absent");
    ok &= check(result.error.isEmpty() && result.result.toObject().value("manifests").toArray().first().toObject().value("resources").toArray().last().toObject().value("state") == "absent"
        && !zipHas(QDir(absent).filePath("download.zip"), "pv.mp4"), "PV 404 is accepted as absent and omitted from ZIP");
    result = download("empty", false, true);
    const auto empty = chartDirectoryPathForTitle(output.path(), "empty", "empty");
    ok &= check(result.error.isEmpty() && QFileInfo(QDir(empty).filePath("maidata.txt")).size() == 0
        && zipHas(QDir(empty).filePath("download.zip"), "maidata.txt"), "declared empty chart shares download and ZIP acceptance");
    result = download("complete", true, false);
    ok &= check(result.result.toObject().value("counts").toObject().value("failed") == 1
        && QFileInfo::exists(QDir(directory).filePath("pv.mp4")), "publication conflict preserves existing output");
    server.statuses.insert("/maichart/retry/track", 500);
    result = download("retry", false, false);
    ok &= check(server.paths.count("/maichart/retry/track") == 3
        && result.result.toObject().value("counts").toObject().value("failed") == 1, "resource retries are bounded to three attempts");
    server.statuses.insert("/maichart/blocked/track", 403);
    result = download("blocked", true, false);
    ok &= check(result.blocked && server.paths.count("/maichart/blocked/chart") == 0, "blocked response stops resource queue");
    const QJsonObject prepare{{"providerId", "majdata"}, {"chartId", "absent"}, {"remoteVersion", "v1"}};
    auto preview = run(provider, "net.previews.prepare", prepare);
    const int requests = server.paths.size();
    auto cached = run(provider, "net.previews.prepare", prepare);
    const auto previewRef = preview.result.toObject().value("previewRef").toString();
    ok &= check(preview.error.isEmpty() && previewRef == cached.result.toObject().value("previewRef") && server.paths.size() == requests
        && preview.result.toObject().value("manifest").toObject().value("remoteVersion") == "v1"
        && !provider.previewPath("desktop", previewRef).isEmpty() && provider.previewPath("other", previewRef).isEmpty(),
        "preview cache includes absent video and enforces principal ownership");
    auto version = prepare; version.insert("remoteVersion", "v2");
    cached = run(provider, "net.previews.prepare", version);
    ok &= check(cached.result.toObject().value("previewRef") != previewRef, "remote version invalidates preview cache identity");
    const auto released = provider.call("desktop", "net.previews.release", {{"previewRef", previewRef}});
    ok &= check(released.ok() && provider.previewPath("desktop", previewRef).isEmpty(), "release revokes preview handle");
    auto unauthorized = run(provider, "net.downloads.create", {{"providerId", "majdata"}, {"chartIds", QJsonArray{"denied"}},
        {"destination", QJsonObject{{"kind", "directory_grant"}, {"grantRef", "unknown"}}}}, {"item_1"});
    ok &= check(unauthorized.error.value("code") == "permission.denied", "ungranted destination is rejected");

    QEventLoop cancelLoop;
    NetTaskResult cancellation;
    NetEnginePort::Cancel cancel;
    cancel = provider.execute({"desktop", "net.downloads.create", {{"providerId", "majdata"},
        {"chartIds", QJsonArray{"retained", "never_started"}},
        {"destination", QJsonObject{{"kind", "directory_grant"}, {"grantRef", grant}}}, {"includeVideo", false}}, {"item_1", "item_2"}},
        [&](NetTaskEvent event) { if (event.state == "succeeded") QTimer::singleShot(0, &cancelLoop, [&] { cancel(); }); },
        [&](NetTaskResult value) { cancellation = value; cancelLoop.quit(); });
    QTimer::singleShot(10000, &cancelLoop, [&] { cancel(); cancelLoop.quit(); });
    cancelLoop.exec();
    ok &= check(cancellation.error.value("code") == "job.cancelled"
        && QFileInfo::exists(QDir(chartDirectoryPathForTitle(output.path(), "retained", "retained")).filePath("maidata.txt"))
        && server.paths.count("/maichart/never_started/track") == 0, "cancellation retains published chart and stops remaining items");
    qint64 outputBytes = 0;
    QDirIterator outputFiles(output.path(), QDir::Files, QDirIterator::Subdirectories);
    while (outputFiles.hasNext()) { outputFiles.next(); outputBytes += outputFiles.fileInfo().size(); }
    ok &= check(outputBytes <= 64 * 1024, "download fixture output stays within 64 KiB");
    QTextStream(stdout) << "Download fixture output: " << outputBytes << " bytes\n";
    return ok ? 0 : 1;
}
