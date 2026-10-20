#pragma once

#include "media_tools/net/NetEnginePort.h"
#include "media_tools/net/NetQueryRules.h"
#include "media_tools/net/NetTransportPort.h"
#include "media_tools/net/NetBatchUploadScanner.h"

#include <QElapsedTimer>
#include <QObject>
#include <QQueue>
#include <QTemporaryDir>
#include <memory>

namespace miacode::net {

class NetProvider final : public QObject, public NetEnginePort {
public:
    NetProvider(NetTransportPort& transport, QObject* parent = nullptr,
                QUrl baseUrl = QUrl(QStringLiteral("https://majdata.net/api3/api")));
    ~NetProvider() override;
    bool supports(const QString& operation) const override;
    Cancel execute(NetTaskRequest request, Event event, Done done) override;
    NetCallResult call(const QString& principal, const QString& operation,
                       const QJsonObject& parameters) override;
    QString grantDirectory(const QString& principal, const QString& path) override;
    QString previewPath(const QString& principal, const QString& reference) const override;
    QString uploadMaterialPath(const QString& principal, const QString& planRef, const QString& itemId) const override;
    void setAccountRemembered(const QString& principal, const QString& accountRef, bool remembered) override;

private:
    friend class NetQueryOperation;
    friend class NetResourceOperation;
    friend class NetAccountOperation;
    friend class NetUploadOperation;
    friend class NetScanOperation;
    Cancel executeUploadOperation(NetTaskRequest request, Event event, Done done);
    NetCallResult accountCall(const QString& principal, const QString& operation, const QJsonObject& parameters);
    struct Account { QString principal; QJsonObject descriptor; };
    struct UploadMaterial { NetUploadJob files; QJsonObject descriptor; };
    struct UploadPlan { QString principal; QList<UploadMaterial> materials; qint64 expiresAt; };
    QHash<QString, Account> accounts_;
    QHash<QString, UploadPlan> uploadPlans_;
    QHash<QString, Cancel> accountTransfers_;
    bool uploadActive_ = false;
    qint64 uploadReadyAt_ = 0;
    qint64 accountRetryAt_ = 0;
    Cancel executeResources(NetTaskRequest request, Event event, Done done);
    void startNextResource();
    QQueue<std::function<void()>> resourceQueue_;
    bool resourceActive_ = false;
    QJsonObject registerArtifact(const QString& principal, const QString& path, bool directory);
    struct StoredPath { QString principal; QString path; };
    struct Preview { QString principal; QString path; QJsonObject handle; qint64 expiresAt; };
    QHash<QString, StoredPath> directoryGrants_;
    QHash<QString, StoredPath> artifacts_;
    QHash<QString, Preview> previews_;
    QHash<QString, QString> previewCache_;
    std::unique_ptr<QTemporaryDir> previewRoot_;
    QJsonObject storeQuery(const QString& principal, const QList<NetChartSummary>& charts, int skippedRows);
    NetCallResult queryPage(const QString& principal, const QJsonObject& parameters);
    struct QuerySnapshot {
        QString principal;
        QJsonArray charts;
        qint64 expiresAt = 0;
    };
    struct Cursor {
        QString queryRef;
        int offset = 0;
    };
    NetTransportPort& transport_;
    QUrl baseUrl_;
    QElapsedTimer clock_;
    NetCandidateCache cache_;
    QHash<QString, QuerySnapshot> queries_;
    QHash<QString, Cursor> cursors_;
};

} // namespace miacode::net
