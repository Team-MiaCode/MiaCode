#pragma once

#include <QAbstractListModel>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QUrl>
#include <QVariantList>

#include <atomic>
#include <memory>

class QThread;

#include "batch_cleaner/BatchCleanupCore.h"

namespace miacode::batch_cleaner {

class BatchJobListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        TitleRole = Qt::UserRole + 1,
        PathRole,
        CheckedRole,
        StatusRole,
        StatusTextRole,
        DifficultyCountRole,
        CommentCountRole,
        DetailRole,
    };

    struct Entry {
        ChartJob job;
        bool checked = true;
        // pending, changed, unchanged, warning, failed, written
        QString status = QStringLiteral("pending");
        QString title;
        int difficultyCount = -1;
        int commentCount = -1;
        QString detail;
    };

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    const QVector<Entry>& entries() const { return entries_; }
    Entry& entryAt(int row) { return entries_[row]; }
    void setEntries(QVector<Entry> entries);
    void removeRows(const QList<int>& rows);
    void touch(int row);
    void touchAll();

private:
    QVector<Entry> entries_;
};

class BatchCleanerController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject* jobs READ jobs CONSTANT)
    Q_PROPERTY(int jobCount READ jobCount NOTIFY jobsChanged)
    Q_PROPERTY(int checkedCount READ checkedCount NOTIFY jobsChanged)
    Q_PROPERTY(QString outputRoot READ outputRoot WRITE setOutputRoot NOTIFY outputRootChanged)
    Q_PROPERTY(bool normalize READ normalize WRITE setNormalize NOTIFY optionsChanged)
    Q_PROPERTY(bool reduceTo384Grid READ reduceTo384Grid WRITE setReduceTo384Grid NOTIFY optionsChanged)
    Q_PROPERTY(int sectionIndex READ sectionIndex WRITE setSectionIndex NOTIFY optionsChanged)
    Q_PROPERTY(int syntaxIndex READ syntaxIndex WRITE setSyntaxIndex NOTIFY optionsChanged)
    Q_PROPERTY(bool stripComments READ stripComments WRITE setStripComments NOTIFY optionsChanged)
    Q_PROPERTY(bool stripTimeSignatures READ stripTimeSignatures WRITE setStripTimeSignatures NOTIFY optionsChanged)
    Q_PROPERTY(bool unifyDesigner READ unifyDesigner WRITE setUnifyDesigner NOTIFY optionsChanged)
    Q_PROPERTY(QString designer READ designer WRITE setDesigner NOTIFY optionsChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool writing READ writing NOTIFY busyChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(int progressTotal READ progressTotal NOTIFY progressChanged)
    Q_PROPERTY(QString summary READ summary NOTIFY summaryChanged)
    Q_PROPERTY(QStringList messages READ messages NOTIFY summaryChanged)
    Q_PROPERTY(int selectedRow READ selectedRow WRITE setSelectedRow NOTIFY previewChanged)
    Q_PROPERTY(QStringList previewSections READ previewSections NOTIFY previewChanged)
    Q_PROPERTY(int previewSection READ previewSection WRITE setPreviewSection NOTIFY previewChanged)
    Q_PROPERTY(QVariantList previewOriginal READ previewOriginal NOTIFY previewChanged)
    Q_PROPERTY(QVariantList previewCleaned READ previewCleaned NOTIFY previewChanged)

public:
    explicit BatchCleanerController(QObject* parent = nullptr);
    ~BatchCleanerController() override;

    QObject* jobs() { return &model_; }
    int jobCount() const { return model_.rowCount(); }
    int checkedCount() const;
    QString outputRoot() const { return outputRoot_; }
    void setOutputRoot(const QString& path);

    bool normalize() const { return options_.normalize; }
    void setNormalize(bool value);
    bool reduceTo384Grid() const { return options_.normalization.reduceTo384Grid; }
    void setReduceTo384Grid(bool value);
    int sectionIndex() const;
    void setSectionIndex(int index);
    int syntaxIndex() const;
    void setSyntaxIndex(int index);
    bool stripComments() const { return options_.stripComments; }
    void setStripComments(bool value);
    bool stripTimeSignatures() const { return options_.stripTimeSignatures; }
    void setStripTimeSignatures(bool value);
    bool unifyDesigner() const { return options_.unifyDesigner; }
    void setUnifyDesigner(bool value);
    QString designer() const { return options_.designer; }
    void setDesigner(const QString& value);

    bool busy() const { return busy_; }
    bool writing() const { return writing_; }
    int progress() const { return progress_; }
    int progressTotal() const { return progressTotal_; }
    QString summary() const { return summary_; }
    QStringList messages() const { return messages_; }

    int selectedRow() const { return selectedRow_; }
    void setSelectedRow(int row);
    QStringList previewSections() const { return previewSections_; }
    int previewSection() const { return previewSection_; }
    void setPreviewSection(int section);
    QVariantList previewOriginal() const { return previewOriginal_; }
    QVariantList previewCleaned() const { return previewCleaned_; }

    Q_INVOKABLE void addUrls(const QList<QUrl>& urls);
    Q_INVOKABLE void addPaths(const QStringList& paths);
    Q_INVOKABLE void setChecked(int row, bool checked);
    Q_INVOKABLE void setAllChecked(bool checked);
    Q_INVOKABLE void removeRow(int row);
    Q_INVOKABLE void removeChecked();
    Q_INVOKABLE void clear();
    Q_INVOKABLE void preflight();
    Q_INVOKABLE void run();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void openOutputRoot() const;
    Q_INVOKABLE QString pathFromUrl(const QUrl& url) const;
    Q_INVOKABLE QUrl urlFromPath(const QString& path) const;

    CleanupOptions cleanupOptions() const { return options_; }

signals:
    void jobsChanged();
    void outputRootChanged();
    void optionsChanged();
    void busyChanged();
    void progressChanged();
    void summaryChanged();
    void previewChanged();

private:
    void rebuildJobs();
    void start(bool write);
    void applyResult(quint64 generation, int row, const JobResult& result, bool write);
    void finish(quint64 generation, bool write, bool cancelled);
    void optionsEdited();
    void invalidateResults();
    void refreshPreview();
    void saveSettings() const;

    BatchJobListModel model_;
    QStringList inputs_;
    QSet<QString> removedCharts_;
    QString outputRoot_;
    bool outputRootEdited_ = false;
    CleanupOptions options_;

    bool busy_ = false;
    bool writing_ = false;
    int progress_ = 0;
    int progressTotal_ = 0;
    QString summary_;
    QStringList messages_;
    QStringList collectErrors_;
    quint64 generation_ = 0;
    std::shared_ptr<std::atomic_bool> cancelFlag_;
    QThread* worker_ = nullptr;

    int selectedRow_ = -1;
    QStringList previewSections_;
    int previewSection_ = 0;
    QVariantList previewOriginal_;
    QVariantList previewCleaned_;
    quint64 previewGeneration_ = 0;
    QList<QThread*> previewThreads_;
};

}  // namespace miacode::batch_cleaner
