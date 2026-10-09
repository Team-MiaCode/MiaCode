#include "MobileCoverComposition.h"
#include "AndroidDocumentSession.h"
#include "MobilePreview.h"
#include "MobileExportComposition.h"
#include "ExportDestination.h"
#include <QJsonDocument>
#include <QTimer>
#include <QUuid>
#include <QGuiApplication>
#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QJniEnvironment>
#endif

namespace miacode::android {
MobileCoverComposition::MobileCoverComposition(AndroidDocumentSession& document,
    MobilePreview& preview, MobileExportComposition& exportComposition)
    : document_(document), playbackSlot_(&preview),
      session_(exportComposition, *qobject_cast<UiRequestService*>(exportComposition.requests()), playbackSlot_),
      batch_(exportComposition, session_)
{
    session_.setFilePublisher([this](const QString& path, auto callback) { publish(path, std::move(callback)); });
    batch_.setFilePublisher([this](const QString& path, auto callback) { publish(path, std::move(callback)); },
        [this] { cancelPublications(); });
    batch_.setExecutionHooks([this, &exportComposition, &preview](QString* error) {
        if (exportComposition.exportActive()) {
            *error = qtTrId("cover.batch_other_export"); return false;
        }
        if (!document_.backgroundExportAllowed() && qGuiApp->applicationState() != Qt::ApplicationActive) {
            *error = qtTrId("cover.batch_foreground_required"); return false;
        }
#ifdef Q_OS_ANDROID
        if (document_.backgroundExportAllowed()) {
            QJniObject::callStaticMethod<void>("org/miacode/android/ChartExportService", "begin", "(Landroid/content/Context;)V",
                QNativeInterface::QAndroidApplication::context().object());
            QJniEnvironment env;
            if (env.checkAndClearExceptions()) { *error = qtTrId("cover.batch_service_failed"); return false; }
            backgroundServiceActive_ = true;
        }
#endif
        if (preview.playing()) preview.togglePlayback();
        return true;
    }, [this] { endBackgroundService(); }, [this](int percent) {
#ifdef Q_OS_ANDROID
        if (backgroundServiceActive_) {
            QJniObject::callStaticMethod<void>("org/miacode/android/ChartExportService", "progress", "(I)V", percent);
            QJniEnvironment env;
            env.checkAndClearExceptions();
        }
#else
        Q_UNUSED(percent);
#endif
    });
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (batch_.running() && state != Qt::ApplicationActive && !document_.backgroundExportAllowed()) batch_.cancel();
    });
    connect(&document, &AndroidDocumentSession::documentReplaced, this, [this] {
        batch_.cancel();
        // Drop media and frame caches too; a replacement may have no difficulty.
        session_.invalidateDocument();
    });
}
MobileCoverComposition::~MobileCoverComposition()
{
    batch_.cancel();
    cancelPublications();
    endBackgroundService();
    batch_.setFilePublisher({}, {});
    batch_.setExecutionHooks({}, {}, {});
    session_.leave();
}
void MobileCoverComposition::cancelPublications()
{
#ifdef Q_OS_ANDROID
    for (auto it = publications_.cbegin(); it != publications_.cend(); ++it)
        QJniObject::callStaticMethod<void>("org/miacode/android/ExportFilePublisher", "cancel",
            "(Ljava/lang/String;)V", QJniObject::fromString(it.key()).object<jstring>());
#endif
}
void MobileCoverComposition::endBackgroundService()
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
void MobileCoverComposition::enter()
{
    session_.enter(document_.activeDifficulty());
}
void MobileCoverComposition::publish(const QString& path, ui::CoverExportSession::PublicationResult callback)
{
#ifdef Q_OS_ANDROID
    const auto destination = exportDestination(path);
    if (destination.isEmpty()) {
        callback(false, path, tr("Please select an Android output folder; the private export is retained."));
        return;
    }
    const auto token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    publications_.insert(token, std::move(callback));
    QJniObject::callStaticMethod<void>("org/miacode/android/ExportFilePublisher", "publish",
        "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
        QNativeInterface::QAndroidApplication::context().object(), QJniObject::fromString(token).object<jstring>(),
        QJniObject::fromString(path).object<jstring>(),
        QJniObject::fromString(QString::fromUtf8(QJsonDocument(destination).toJson(QJsonDocument::Compact))).object<jstring>());
    QJniEnvironment env;
    if (env.checkAndClearExceptions()) {
        auto failed = publications_.take(token);
        failed(false, path, tr("Cannot publish the cover; the private export is retained."));
    }
#else
    QTimer::singleShot(0, this, [path, callback = std::move(callback)] { callback(true, path, {}); });
#endif
}
void MobileCoverComposition::publicationUpdate(const QJsonObject& result)
{
    if (!result.value("done").toBool()) return;
    const auto token = result.value("token").toString();
    if (!publications_.contains(token)) return;
    auto callback = publications_.take(token);
    callback(result.value("ok").toBool(), result.value("displayPath").toString(), result.value("error").toString());
}
}
