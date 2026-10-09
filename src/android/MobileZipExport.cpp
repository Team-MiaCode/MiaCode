#include "MobileZipExport.h"
#include "ExportDestination.h"
#include <QCoreApplication>
#include <QFile>
#include <QSaveFile>
#include <QUuid>
#include <QJsonDocument>
#include <QPointer>
#include <QGuiApplication>
#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QJniEnvironment>
#endif

namespace miacode::android {
MobileZipExport::MobileZipExport(AndroidDocumentSession& document, UiRequestService& requests,
    JobProgressService& progress, QObject* parent)
    : QObject(parent), document_(document), requests_(requests), progress_(progress)
{
    connect(&progress_, &JobProgressService::cancellationRequested, this, [this](quint64 token) {
        if (running_ && token == progressToken_) cancel();
    });
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, &MobileZipExport::cancel);
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (running_ && state != Qt::ApplicationActive && !document_.backgroundExportAllowed()) cancel();
    });
    connect(&progress_, &JobProgressService::changed, this, [this] {
#ifdef Q_OS_ANDROID
        if (backgroundServiceActive_ && running_ && progress_.token() == progressToken_) {
            QJniObject::callStaticMethod<void>("org/miacode/android/ChartExportService", "progress", "(I)V", progress_.percent());
            QJniEnvironment env;
            env.checkAndClearExceptions();
        }
#endif
    });
}
MobileZipExport::~MobileZipExport()
{
    cancel();
    if (worker_.joinable()) worker_.join();
    endBackgroundService();
}
void MobileZipExport::requestExport()
{
    if (running_ || picking_ || progress_.active() || document_.busy()) return;
    emit document_.editingFinishedRequested();
    if (!document_.hasDocument()) return;
    const auto& chart = document_.workspace().document();
    zip_export::ChartZipInput input{chart.toText(), document_.currentFilePath(), chart.videoPath, {}};
    FileRequest request;
    request.title = qtTrId("export.export_as_zip");
    request.startPath = zip_export::sanitizedZipStem(chart.title) + QStringLiteral(".zip");
    request.nameFilters = {QStringLiteral("ZIP (*.zip)")};
    request.saveMode = true;
    picking_ = true;
    QPointer<MobileZipExport> guard(this);
    requests_.requestFile(request, [guard, input](const QString& path) {
        if (!guard) return;
        guard->picking_ = false;
        if (path.isEmpty()) return;
        QString output = path;
        if (!output.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive)) {
            output += QStringLiteral(".zip");
            const auto destination = exportDestination(path);
            if (!destination.isEmpty()) rememberExportDestination(output, destination.value("uri").toString(),
                destination.value("tree").toBool(), destination.value("displayPath").toString());
        }
        guard->start(input, output);
    });
}
void MobileZipExport::start(zip_export::ChartZipInput input, const QString& path)
{
    if (running_ || progress_.active()) {
        requests_.postNotice(NoticeSeverity::Warning, qtTrId("export.export_as_zip"), tr("另一个导出任务正在进行，请稍后重试。"));
        return;
    }
    if (worker_.joinable()) worker_.join();
    running_ = true;
    cancelled_.store(false);
    entries_.clear();
    publicationToken_.clear();
    outputPath_ = path;
    displayPath_ = exportDestinationDisplayPath(path);
    progressToken_ = progress_.begin(qtTrId("export.export_as_zip"), qtTrId("export.preparing_package"), true);
    emit changed();
#ifdef Q_OS_ANDROID
    if (document_.backgroundExportAllowed()) {
        QJniObject::callStaticMethod<void>("org/miacode/android/ChartExportService", "begin", "(Landroid/content/Context;)V",
            QNativeInterface::QAndroidApplication::context().object());
        QJniEnvironment env;
        if (env.checkAndClearExceptions()) { finish(false, tr("无法启动后台导出服务，请回到前台重试。")); return; }
        backgroundServiceActive_ = true;
    }
#endif
    input.outputZipPath = path + QStringLiteral(".packing-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    worker_ = std::thread([this, input, path] {
        auto result = zip_export::packChartToZip(input, [this](int current, int total, const QString& name) {
            QMetaObject::invokeMethod(this, [this, current, total, name] {
                if (running_ && progress_.token() == progressToken_)
                    progress_.report((current - 1) * 75 / qMax(1, total),
                        qtTrId("export.packaging_1_2_3").arg(current).arg(total).arg(name));
            }, Qt::QueuedConnection);
            return !cancelled_.load();
        });
        if (result.ok) {
            QFile source(input.outputZipPath);
            QSaveFile target(path);
            bool copied = source.open(QIODevice::ReadOnly) && target.open(QIODevice::WriteOnly);
            while (copied && !source.atEnd() && !cancelled_.load()) {
                const auto bytes = source.read(1024 * 1024);
                copied = !bytes.isEmpty() && target.write(bytes) == bytes.size();
            }
            result.canceled = cancelled_.load();
            result.ok = copied && !result.canceled && target.commit();
            if (!result.ok && !result.canceled)
                result.errorMessage = tr("无法提交 ZIP 文件；请检查剩余空间与写入权限。");
            source.close();
        }
        QFile::remove(input.outputZipPath);
        QMetaObject::invokeMethod(this, [this, result] { packed(result); }, Qt::QueuedConnection);
    });
}
void MobileZipExport::packed(const zip_export::ChartZipResult& result)
{
    if (!running_) return;
    if (!result.ok || result.canceled || cancelled_.load()) {
        finish(false, result.canceled || cancelled_.load() ? qtTrId("export.packaging_canceled") : result.errorMessage);
        return;
    }
    entries_ = result.includedEntries;
#ifdef Q_OS_ANDROID
    const auto destination = exportDestination(outputPath_);
    if (destination.isEmpty()) { finish(false, tr("未找到安卓导出目标；完整 ZIP 已保留在应用内。")); return; }
    publicationToken_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (progress_.token() == progressToken_) progress_.report(80, displayPath_);
    QJniObject::callStaticMethod<void>("org/miacode/android/ExportFilePublisher", "publish",
        "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
        QNativeInterface::QAndroidApplication::context().object(), QJniObject::fromString(publicationToken_).object<jstring>(),
        QJniObject::fromString(outputPath_).object<jstring>(),
        QJniObject::fromString(QString::fromUtf8(QJsonDocument(destination).toJson(QJsonDocument::Compact))).object<jstring>());
    QJniEnvironment env;
    if (env.checkAndClearExceptions()) finish(false, tr("无法发布 ZIP；完整文件已保留在应用内。"));
#else
    finish(true);
#endif
}
void MobileZipExport::publicationUpdate(const QJsonObject& result)
{
    if (!running_ || publicationToken_.isEmpty() || result.value("token").toString() != publicationToken_) return;
    if (!result.value("done").toBool()) {
        if (progress_.token() == progressToken_) progress_.report(80 + result.value("percent").toInt() / 5, displayPath_);
        return;
    }
    const auto label = result.value("displayPath").toString();
    if (!label.isEmpty()) displayPath_ = label;
    finish(result.value("ok").toBool() && !cancelled_.load(), result.value("error").toString());
}
void MobileZipExport::cancel()
{
    cancelled_.store(true);
#ifdef Q_OS_ANDROID
    if (!publicationToken_.isEmpty()) QJniObject::callStaticMethod<void>("org/miacode/android/ExportFilePublisher", "cancel",
        "(Ljava/lang/String;)V", QJniObject::fromString(publicationToken_).object<jstring>());
#endif
}
void MobileZipExport::endBackgroundService()
{
#ifdef Q_OS_ANDROID
    if (backgroundServiceActive_) {
        QJniObject::callStaticMethod<void>("org/miacode/android/ChartExportService", "end", "(Landroid/content/Context;)V",
            QNativeInterface::QAndroidApplication::context().object());
        QJniEnvironment env;
        env.checkAndClearExceptions();
    }
#endif
    backgroundServiceActive_ = false;
}
void MobileZipExport::finish(bool success, const QString& error)
{
    running_ = false;
    publicationToken_.clear();
    endBackgroundService();
    if (progress_.token() == progressToken_) progress_.end();
    const QString message = success
        ? qtTrId("export.exported_to_1_2_file").arg(displayPath_).arg(entries_.size()).arg(QString())
        : (error.isEmpty() ? qtTrId("export.packaging_canceled") : error);
    requests_.postNotice(success || cancelled_.load() ? NoticeSeverity::Information : NoticeSeverity::Error,
        qtTrId("export.export_as_zip"), message, entries_.join(QLatin1Char('\n')));
    emit changed();
    emit finished(success, outputPath_, success ? QString() : message, entries_);
}
}
