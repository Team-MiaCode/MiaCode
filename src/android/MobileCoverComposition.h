#pragma once

#include "app/ui/export/CoverExportSession.h"
#include "tools/cover_export/CoverBatchExport.h"
#include <QHash>
#include <QJsonObject>

namespace miacode::android {
class AndroidDocumentSession;
class MobilePreview;
class MobileExportComposition;

// Platform wiring around the production cover session; layer/preset/render
// semantics stay with the v2 owners.
class MobileCoverComposition final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* session READ session CONSTANT)
    Q_PROPERTY(QObject* batch READ batch CONSTANT)
public:
    MobileCoverComposition(AndroidDocumentSession&, MobilePreview&, MobileExportComposition&);
    ~MobileCoverComposition() override;
    QObject* session() { return &session_; }
    ui::CoverExportSession& coverSession() { return session_; }
    QObject* batch() { return &batch_; }
    cover_export::CoverBatchExport& batchController() { return batch_; }
    void cancelBatch() { batch_.cancel(); }
    Q_INVOKABLE void enter();
    Q_INVOKABLE void leave() { session_.leave(); }
    void publicationUpdate(const QJsonObject& result);
private:
    void publish(const QString& path, ui::CoverExportSession::PublicationResult callback);
    void cancelPublications();
    void endBackgroundService();
    AndroidDocumentSession& document_;
    PlaybackControl* playbackSlot_;
    ui::CoverExportSession session_;
    cover_export::CoverBatchExport batch_;
    bool backgroundServiceActive_ = false;
    QHash<QString, ui::CoverExportSession::PublicationResult> publications_;
};
}
