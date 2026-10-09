#pragma once

#include "AndroidDocumentSession.h"
#include "app/services/JobProgressService.h"
#include "app/services/UiRequestService.h"
#include "tools/zip_export/ChartZipPackager.h"
#include <atomic>
#include <thread>

namespace miacode::android {
class MobileZipExport final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY changed)
public:
    MobileZipExport(AndroidDocumentSession&, UiRequestService&, JobProgressService&, QObject* parent = nullptr);
    ~MobileZipExport() override;
    bool running() const { return running_; }
    Q_INVOKABLE void requestExport();
    Q_INVOKABLE void cancel();
    void publicationUpdate(const QJsonObject&);
signals:
    void changed();
    void finished(bool success, const QString& path, const QString& error, const QStringList& entries);
private:
    void start(zip_export::ChartZipInput input, const QString& path);
    void packed(const zip_export::ChartZipResult&);
    void finish(bool success, const QString& error = {});
    void endBackgroundService();
    AndroidDocumentSession& document_;
    UiRequestService& requests_;
    JobProgressService& progress_;
    std::thread worker_;
    std::atomic_bool cancelled_{false};
    bool picking_ = false;
    bool running_ = false;
    bool backgroundServiceActive_ = false;
    quint64 progressToken_ = 0;
    QString publicationToken_;
    QString outputPath_;
    QString displayPath_;
    QStringList entries_;
};
}
