#include "android/MobileZipExport.h"
#include <QGuiApplication>
#include <QEventLoop>
#include <QTemporaryDir>
#include <QFile>
#include <QTimer>
#include <miniz.h>
#include <cstdio>
#include <cstring>

using namespace miacode;
using namespace miacode::android;
namespace {
int failures = 0;
void check(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); ++failures; }
}
bool writeFile(const QString& path, const QByteArray& data) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}
QByteArray readFile(const QString& path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
QByteArray archivedText(const QString& path) {
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_file(&zip, path.toUtf8().constData(), 0)) return {};
    size_t size = 0;
    void* bytes = mz_zip_reader_extract_file_to_heap(&zip, "maidata.txt", &size, 0);
    const auto result = bytes ? QByteArray(static_cast<const char*>(bytes), static_cast<int>(size)) : QByteArray();
    mz_free(bytes);
    mz_zip_reader_end(&zip);
    return result;
}
}
int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    QTemporaryDir temporary;
    check(temporary.isValid(), "temporary storage available");
    AndroidDocumentSession document(temporary.path() + "/storage");
    document.setTitle(QString::fromUtf8("离线 ZIP"));
    document.setChartText("(120){4}1,2,3,4,E");
    UiRequestService requests;
    JobProgressService progress;
    MobileZipExport exporter(document, requests, progress);
    QString requestId;
    QVariantMap fileRequest;
    int requestCount = 0, finishedCount = 0;
    bool succeeded = false, cancelAtBegin = false;
    QString error;
    QObject::connect(&requests, &UiRequestService::fileRequested, &app, [&](const QString& id, const QVariantMap& data) {
        requestId = id; fileRequest = data; ++requestCount;
    });
    QObject::connect(&requests, &UiRequestService::noticeRequested, &app, [&](const QString& id, const QVariantMap&) {
        requests.submitNoticeResult(id, false);
    });
    QObject::connect(&exporter, &MobileZipExport::finished, &app, [&](bool success, const QString&, const QString& failure, const QStringList&) {
        succeeded = success; error = failure; ++finishedCount;
    });
    QObject::connect(&progress, &JobProgressService::changed, &app, [&] {
        if (cancelAtBegin && progress.active()) exporter.cancel();
    });
    const auto waitFor = [&](const std::function<void()>& action) {
        QEventLoop loop;
        const auto connection = QObject::connect(&exporter, &MobileZipExport::finished, &loop, &QEventLoop::quit);
        QTimer timeout;
        timeout.setSingleShot(true);
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        timeout.start(5000);
        const int before = finishedCount;
        action();
        if (finishedCount == before) loop.exec();
        QObject::disconnect(connection);
        check(finishedCount == before + 1, "job finishes exactly once within deadline");
    };

    // Pending editor fields belong to the request snapshot, even if the document changes while picking.
    const auto flush = QObject::connect(&document, &AndroidDocumentSession::editingFinishedRequested, &app, [&] {
        document.setChartText("(120){4}1,2,3,4,5,E");
    });
    exporter.requestExport();
    const QByteArray captured = document.workspace().document().toText().toUtf8();
    QObject::disconnect(flush);
    check(fileRequest.value("saveMode").toBool() && fileRequest.value("nameFilters").toStringList().contains("ZIP (*.zip)"),
        "ZIP uses the shared save-file request contract");
    exporter.requestExport();
    check(requestCount == 1, "duplicate picker requests are ignored");
    document.setChartText("(180){8}8,7,6,5,E");
    const auto revision = document.documentRevision();
    const QString output = temporary.path() + "/snapshot";
    waitFor([&] { requests.submitFileResult(requestId, QUrl::fromLocalFile(output)); });
    check(succeeded && archivedText(output + ".zip") == captured, "archive contains flushed snapshot and adds ZIP extension");
    check(document.documentRevision() == revision && document.chartText() == "(180){8}8,7,6,5,E",
        "export does not replace later edits or their revision");
    check(!progress.active() && !exporter.running(), "successful job releases the progress overlay");

    exporter.requestExport();
    requests.cancelFileRequest(requestId);
    check(finishedCount == 1 && !exporter.running(), "picker cancellation launches no job");

    const QString retained = temporary.path() + "/retained.zip";
    check(writeFile(retained, "existing output"), "existing destination prepared");
    cancelAtBegin = true;
    exporter.requestExport();
    waitFor([&] { requests.submitFileResult(requestId, QUrl::fromLocalFile(retained)); });
    cancelAtBegin = false;
    check(!succeeded && !error.isEmpty() && readFile(retained) == "existing output", "cancellation preserves existing output");
    check(!progress.active(), "cancelled job releases progress overlay");

    const QString blocked = temporary.path() + "/not-a-directory";
    check(writeFile(blocked, "sentinel"), "invalid parent prepared");
    exporter.requestExport();
    waitFor([&] { requests.submitFileResult(requestId, QUrl::fromLocalFile(blocked + "/failed.zip")); });
    check(!succeeded && !error.isEmpty() && readFile(blocked) == "sentinel", "write failure is reported and preserves surrounding files");

    exporter.requestExport();
    const QByteArray latest = document.workspace().document().toText().toUtf8();
    waitFor([&] { requests.submitFileResult(requestId, QUrl::fromLocalFile(retained)); });
    check(succeeded && archivedText(retained) == latest && !progress.active(), "failed and cancelled jobs can retry and replace output atomically");
    std::printf("Mobile ZIP snapshot, cancellation, failure and retry: %s\n", failures ? "failed" : "passed");
    return failures ? 1 : 0;
}
