#pragma once

#include "app/ui/export/CoverExportSession.h"
#include <QList>
#include <QVariantList>

namespace miacode::cover_export {
class SceneFrameRenderer;

// A bounded, sequential GUI-thread queue. All tasks and compositions are copied
// before execution; offscreen Quick renderers never borrow the live cover scene.
class CoverBatchExport final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY changed)
    Q_PROPERTY(bool cancelRequested READ cancelRequested NOTIFY changed)
    Q_PROPERTY(QVariantList presets READ presets NOTIFY optionsChanged)
    Q_PROPERTY(QVariantList results READ results NOTIFY changed)
    Q_PROPERTY(int completed READ completed NOTIFY changed)
    Q_PROPERTY(int total READ total NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    CoverBatchExport(ExportEngine&, ui::CoverExportSession&, QObject* parent = nullptr);
    ~CoverBatchExport() override;
    bool running() const { return running_; }
    bool cancelRequested() const { return canceled_; }
    QVariantList presets() const;
    QVariantList results() const;
    int completed() const { return completed_; }
    int total() const { return static_cast<int>(jobs_.size()); }
    QString error() const { return error_; }
    using Begin = std::function<bool(QString*)>;
    void setExecutionHooks(Begin begin, std::function<void()> end,
                           std::function<void(int)> progress);
    void setFilePublisher(ui::CoverExportSession::FilePublisher publisher,
                          std::function<void()> cancelPublication);
    Q_INVOKABLE bool start(const QVariantList& difficulties, const QVariantList& presets);
    Q_INVOKABLE void cancel();
signals:
    void changed();
    void optionsChanged();
    void finished();
private:
    struct Job {
        VideoExportTask task;
        QJsonObject composition;
        CoverComposerInputs inputs;
        QString label, baseName, status = QStringLiteral("queued"), path, error;
    };
    void advance();
    void completeCurrent(bool success, const QString& path, const QString& error);
    void finish();
    ExportEngine& engine_;
    ui::CoverExportSession& session_;
    QList<Job> jobs_;
    int completed_ = 0;
    bool running_ = false, canceled_ = false;
    quint64 generation_ = 0;
    QString error_;
    Begin begin_;
    std::function<void()> end_, cancelPublication_;
    std::function<void(int)> progress_;
    ui::CoverExportSession::FilePublisher publisher_;
    std::unique_ptr<SceneFrameRenderer> frameRenderer_;
    std::unique_ptr<PreviewQuickExportSession> compositeRenderer_;
};
}
