#pragma once

#include "app/services/net/NetService.h"
#include "app/services/api/ApiDispatcher.h"

#include <QAbstractListModel>
#include <QPointer>
#include <QElapsedTimer>
#include <QSet>
#include <QVariantMap>
#include <memory>

namespace miacode { class UiRequestService; }

namespace miacode::ui {
class DocumentModel;

class NetModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool probing READ probing NOTIFY changed)
    Q_PROPERTY(int resultCount READ rowCount NOTIFY changed)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY changed)
    Q_PROPERTY(QString statusText READ statusText NOTIFY changed)
    Q_PROPERTY(QString errorText READ errorText NOTIFY changed)
    Q_PROPERTY(bool working READ working NOTIFY changed)
    Q_PROPERTY(bool downloading READ downloading NOTIFY changed)
    Q_PROPERTY(bool previewing READ previewing NOTIFY changed)
    Q_PROPERTY(bool paused READ paused NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY changed)
    Q_PROPERTY(QString progressText READ progressText NOTIFY changed)
    Q_PROPERTY(QString connectionText READ connectionText NOTIFY changed)
    Q_PROPERTY(QString logText READ logText NOTIFY changed)
    Q_PROPERTY(QString outputDirectory READ outputDirectory WRITE setOutputDirectory NOTIFY changed)

public:
    enum Role { ChartIdRole = Qt::UserRole + 1, TitleRole, ArtistRole, DesignerRole,
        UploaderRole, LevelsRole, TagsRole, UploadedAtRole, SelectedRole, StateRole };
    NetModel(NetService* service, QObject* parent = nullptr);
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    bool busy() const { return !queryJobId_.isEmpty(); }
    bool probing() const { return !probeJobId_.isEmpty(); }
    int selectedCount() const;
    QString statusText() const { return statusText_; }
    QString errorText() const { return errorText_; }
    void setHost(DocumentModel* document, miacode::UiRequestService* requests);
    bool working() const { return busy() || probing() || downloading() || previewing(); }
    bool downloading() const { return !downloadJobId_.isEmpty(); }
    bool previewing() const { return !previewJobId_.isEmpty(); }
    bool paused() const { return paused_; }
    double progress() const { return progress_; }
    QString progressText() const { return progressText_; }
    QString connectionText() const { return connectionText_; }
    QString logText() const { return logs_.join('\n'); }
    QString outputDirectory() const { return outputDirectory_; }
    void setOutputDirectory(const QString& path);

    Q_INVOKABLE void query(const QVariantMap& parameters);
    Q_INVOKABLE void probe();
    Q_INVOKABLE void cancelQuery();
    Q_INVOKABLE void cancelProbe();
    Q_INVOKABLE void setSelected(const QString& chartId, bool selected);
    Q_INVOKABLE void selectAll(bool selected);
    Q_INVOKABLE void setSort(const QString& sort);
    Q_INVOKABLE void browseOutputDirectory();
    Q_INVOKABLE void downloadSelected(bool includeVideo, bool createZip);
    Q_INVOKABLE void preview(const QString& chartId);
    Q_INVOKABLE void cancelTask();
    Q_INVOKABLE void resumeDownload();

private:
    void updateJob(const QString& jobId);
    void loadQuery(const QString& queryRef);
    void updateDownload();
    void appendLog(const QString& text);
    void applySort();
    net::NetCallResult call(const QString& operation, const QJsonObject& parameters = {});
    QPointer<NetService> service_;
    std::unique_ptr<api::ApiDispatcher> api_;
    QJsonArray charts_;
    QSet<QString> selected_;
    QString queryJobId_;
    QString probeJobId_;
    QString statusText_;
    QString errorText_;
    DocumentModel* document_ = nullptr;
    miacode::UiRequestService* requests_ = nullptr;
    QString outputDirectory_, downloadJobId_, previewJobId_, previewChartId_, connectionText_, progressText_;
    QJsonObject previewExpected_;
    QHash<QString, QString> rowStates_;
    QHash<QString, QString> chartStates_;
    QString displaySort_ = QStringLiteral("uploaded_desc");
    QStringList logs_, downloadChartIds_;
    double progress_ = 0;
    bool paused_ = false;
    QElapsedTimer resourceTimer_;
    QString progressResource_;
    const QString principal_ = QStringLiteral("desktop");

signals:
    void changed();
};

} // namespace miacode::ui
