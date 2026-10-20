#pragma once

#include "app/services/jobs/JobRegistry.h"
#include "media_tools/net/NetEnginePort.h"

#include <QObject>
#include <QPointer>

namespace miacode {

class NetService final : public QObject {
    Q_OBJECT
public:
    NetService(JobRegistry& jobs, net::NetEnginePort& engine, QObject* parent = nullptr);
    ~NetService() override;
    net::NetCallResult start(const QString& principal, const QString& operation,
                             const QJsonObject& parameters, const QString& idempotencyKey = {});
    net::NetCallResult call(const QString& principal, const QString& operation,
                            const QJsonObject& parameters = {});
    JobRegistry& jobs() { return jobs_; }
    bool supports(const QString& operation) const {
        if (operation == QLatin1String("document.snapshot")) return static_cast<bool>(documentSnapshot_);
        return operation == QLatin1String("net.previews.open") ? static_cast<bool>(previewOpen_) : engine_.supports(operation);
    }
    using PreviewOpen = std::function<net::NetEnginePort::Cancel(net::NetTaskRequest, net::NetEnginePort::Done)>;
    void setPreviewOpenHandler(PreviewOpen handler) { previewOpen_ = std::move(handler); }
    void setDocumentSnapshotHandler(std::function<QJsonObject()> handler) { documentSnapshot_ = std::move(handler); }
    QString grantDirectory(const QString& principal, const QString& path) { return engine_.grantDirectory(principal, path); }
    QString previewPath(const QString& principal, const QString& reference) const { return engine_.previewPath(principal, reference); }
    QString uploadMaterialPath(const QString& principal, const QString& planRef, const QString& itemId) const { return engine_.uploadMaterialPath(principal, planRef, itemId); }
    static QString kindForOperation(const QString& operation);

private:
    void launch(const JobToken& token, net::NetTaskRequest request);
    JobRegistry& jobs_;
    net::NetEnginePort& engine_;
    PreviewOpen previewOpen_;
    std::function<QJsonObject()> documentSnapshot_;
    struct Accepted {
        QByteArray fingerprint;
        QString jobId;
    };
    QHash<QString, Accepted> idempotency_;
    QHash<QString, net::NetTaskRequest> resumable_;
    QHash<QString, net::NetEnginePort::Cancel> cancellations_;
};

} // namespace miacode
