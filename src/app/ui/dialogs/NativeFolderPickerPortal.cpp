#include "app/ui/dialogs/NativeFolderPicker.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QFile>
#include <QGuiApplication>
#include <QUuid>
#include <QWindow>

namespace miacode::ui {
namespace {

class PortalFolderRequest final : public QObject
{
    Q_OBJECT
public:
    PortalFolderRequest(QObject* lifetime, UiRequestService::NativeFolderResult callback)
        : QObject(lifetime), callback_(std::move(callback)) {}

    ~PortalFolderRequest() override
    {
        if (!requestPath_.isEmpty()) {
            bus_.asyncCall(QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.portal.Desktop"),
                requestPath_, QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Close")));
        }
    }

    void open(QObject* parentWindow, const QString& title, const QUrl& startFolder)
    {
        const QString token = QStringLiteral("miacode_") + QUuid::createUuid().toString(QUuid::Id128);
        QString sender = bus_.baseService().mid(1);
        sender.replace(QLatin1Char('.'), QLatin1Char('_'));
        requestPath_ = QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token);
        if (!bus_.connect(QStringLiteral("org.freedesktop.portal.Desktop"), requestPath_,
                          QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"),
                          this, SLOT(onResponse(uint,QVariantMap)))) {
            finish({}, QStringLiteral("Cannot subscribe to the desktop file chooser portal"));
            return;
        }
        QVariantMap options{{QStringLiteral("handle_token"), token}, {QStringLiteral("multiple"), true},
                            {QStringLiteral("directory"), true}, {QStringLiteral("modal"), true}};
        if (startFolder.isLocalFile())
            options.insert(QStringLiteral("current_folder"), QFile::encodeName(startFolder.toLocalFile()) + '\0');
        QString owner;
        if (auto* window = qobject_cast<QWindow*>(parentWindow);
            window && QGuiApplication::platformName() == QStringLiteral("xcb"))
            owner = QStringLiteral("x11:%1").arg(qulonglong(window->winId()), 0, 16);
        auto call = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.portal.Desktop"),
            QStringLiteral("/org/freedesktop/portal/desktop"),
            QStringLiteral("org.freedesktop.portal.FileChooser"), QStringLiteral("OpenFile"));
        call.setArguments({owner, title, options});
        auto* watcher = new QDBusPendingCallWatcher(bus_.asyncCall(call), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
            const QDBusPendingReply<QDBusObjectPath> reply = *watcher;
            watcher->deleteLater();
            if (reply.isError()) finish({}, reply.error().message());
        });
    }

private slots:
    void onResponse(uint response, const QVariantMap& results)
    {
        QList<QUrl> urls;
        if (response == 0)
            for (const QString& uri : results.value(QStringLiteral("uris")).toStringList()) urls.append(QUrl(uri));
        finish(urls, response <= 1 ? QString() : QStringLiteral("Desktop file chooser portal failed"));
    }

private:
    void finish(const QList<QUrl>& urls, const QString& error)
    {
        requestPath_.clear();
        auto callback = std::move(callback_);
        deleteLater();
        if (callback) callback(urls, error);
    }
    QDBusConnection bus_ = QDBusConnection::sessionBus();
    QString requestPath_;
    UiRequestService::NativeFolderResult callback_;
};
} // namespace

void showNativeFolders(QObject* lifetime, QObject* parentWindow, const QString& title,
                       const QUrl& startFolder, UiRequestService::NativeFolderResult onResolved)
{
    (new PortalFolderRequest(lifetime, std::move(onResolved)))->open(parentWindow, title, startFolder);
}
} // namespace miacode::ui

#include "NativeFolderPickerPortal.moc"
