#pragma once
#include "app/services/net/NetService.h"
#include "app/services/api/ApiDispatcher.h"
#include "app/services/UiRequestService.h"
#include <QAbstractListModel>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <memory>

namespace miacode::ui {
class NetUploadModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString username READ username WRITE setUsername NOTIFY changed)
    Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY changed)
    Q_PROPERTY(bool remember READ remember WRITE setRemember NOTIFY changed)
    Q_PROPERTY(bool secureStorageAvailable READ secureStorageAvailable CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY changed)
    Q_PROPERTY(QString rootDirectory READ rootDirectory WRITE setRootDirectory NOTIFY changed)
    Q_PROPERTY(QString statusText READ statusText NOTIFY changed)
    Q_PROPERTY(QString logText READ logText NOTIFY changed)
    Q_PROPERTY(bool retryAvailable READ retryAvailable NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY changed)
    Q_PROPERTY(int resultCount READ rowCount NOTIFY changed)
public:
    enum Role { NameRole = Qt::UserRole + 1, FilesRole, PathRole, StateRole, SelectedRole };
    NetUploadModel(NetService* service, UiRequestService& requests, QObject* parent = nullptr);
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    QString username() const { return username_; }
    QString password() const { return password_; }
    bool remember() const { return remember_; }
    bool secureStorageAvailable() const;
    void setUsername(const QString& value);
    void setPassword(const QString& value);
    void setRemember(bool value);
    bool busy() const { return !loginJob_.isEmpty() || !scanJob_.isEmpty() || !uploadJob_.isEmpty(); }
    bool loggedIn() const { return !accountRef_.isEmpty(); }
    QString rootDirectory() const { return rootDirectory_; }
    void setRootDirectory(const QString& value);
    QString statusText() const { return statusText_; }
    QString logText() const;
    bool retryAvailable() const;
    double progress() const { return progress_; }
    Q_INVOKABLE void browse();
    Q_INVOKABLE void addDirectory();
    Q_INVOKABLE void selectRow(int row);
    Q_INVOKABLE void removeSelected();
    Q_INVOKABLE void clearQueue();
    Q_INVOKABLE void moveSelected(int direction);
    Q_INVOKABLE void moveSelectedTo(int row);
    Q_INVOKABLE void startRowDrag(int row);
    Q_INVOKABLE void login();
    Q_INVOKABLE void logout();
    Q_INVOKABLE void upload();
    Q_INVOKABLE void cancel();
signals:
    void changed();
private:
    net::NetCallResult call(const QString& operation, const QJsonObject& parameters = {});
    void updateJob(const QString& jobId);
    void report(const QString& text);
    QPointer<NetService> service_;
    UiRequestService& requests_;
    std::unique_ptr<api::ApiDispatcher> api_;
    QString username_, password_, rootDirectory_, accountRef_, planRef_, statusText_;
    QString loginJob_, scanJob_, uploadJob_;
    QString lastUploadJob_;
    QJsonArray rows_;
    QSet<QString> selected_;
    QStringList logs_, uploadIds_;
    bool remember_ = false, uploadAfterLogin_ = false;
    double progress_ = 0;
    QTimer retryTimer_;
};
}
