#pragma once

#include "MobilePreview.h"
#include "MobileExportAudio.h"
#include "tools/video_export/VideoExportController.h"
#include "preview/runtime/PreviewQuickExportSession.h"
#include "preview/runtime/PreviewSceneAssetLoader.h"
#include <QTimer>
#include <thread>
#ifdef Q_OS_ANDROID
#include <QJniObject>
#endif

namespace miacode::android {
// A job captures its chart/settings/media before starting. The live editor can
// continue changing without altering an export already in progress.
class MobileVideoExport final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY changed)
    Q_PROPERTY(int percent READ percent NOTIFY changed)
    Q_PROPERTY(QString outputPath READ outputPath NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    MobileVideoExport(AndroidDocumentSession& document, MobilePreview& preview, QObject* parent = nullptr);
    ~MobileVideoExport() override;
    VideoExportTask buildTask(int difficultyId = 0) const;
    bool start(const VideoExportTask& task, QString* error = nullptr, bool independentChart = false);
    bool beginBatchExecution(QString* error);
    void endBatchExecution();
    void updateBatchProgress(int percent);
    bool batchCancellationRequested() const { return batchActive_ && batchCanceled_; }
    Q_INVOKABLE void cancel();
    void publicationUpdate(const QJsonObject& result);
    bool running() const { return running_; }
    int percent() const { return percent_; }
    QString outputPath() const { return task_.outputPath; }
    QString error() const { return error_; }
    bool cancellationRequested() const { return cancelled_.load(); }
signals:
    void changed();
    void finished(bool success, const QString& output, const QString& error);
private:
    void prepared(const QString& wav, const QString& error, preview::runtime::PreviewSceneAssetLoadResult assets);
    void advance();
    void complete(bool success, const QString& error = {});
    bool commitFile(const QString& source, const QString& destination, QString* error);
    void finishOutput(const QString& source, const QString& destination);
    bool beginBackgroundService(QString* error);
    void endBackgroundService();
    AndroidDocumentSession& document_;
    MobilePreview& preview_;
    VideoExportTask task_;
    video_export::VideoExportAudioRenderPlan plan_;
    preview::scene::PreviewFrameState state_;
    std::unique_ptr<PreviewQuickExportSession> renderer_;
    QTimer timer_;
    std::thread audioWorker_;
    std::atomic_bool cancelled_{false};
    QString directory_, wav_, error_;
    QString publicationToken_, publishedUri_;
    QJsonObject publicationDetails_;
    QImage pendingFrame_;
    int frame_ = 0, percent_ = 0;
    qint64 decodedVideoUs_ = -1;
    bool running_ = false, videoRequested_ = false, inputFinished_ = false;
    bool batchActive_ = false, batchCanceled_ = false, backgroundServiceActive_ = false;
#ifdef Q_OS_ANDROID
    QJniObject encoder_;
#endif
};
}
