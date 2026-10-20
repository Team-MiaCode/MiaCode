#pragma once

#include "editor/SimaiCompletionCatalog.h"

#include <QAbstractListModel>
#include <QList>

namespace miacode::ui {

// Editable rows keep identity while drafts are typed; only confirmed values persist.
class CompletionCandidateModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit CompletionCandidateModel(editor::CompletionCandidateGroup group, QObject* parent = nullptr);
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QStringList savedValues() const;
    void replaceValues(const QStringList& values, bool discardDrafts = false);
    void setHalfWidthInputEnabled(bool enabled);
    Q_INVOKABLE QString normalizeInput(const QString& text) const;
    void retranslateErrors();
    Q_INVOKABLE void setDraft(const QString& key, const QString& text);
    Q_INVOKABLE bool commit(const QString& key);
    Q_INVOKABLE void finishEditing();
    Q_INVOKABLE void prepareRemoval(const QString& key);
    Q_INVOKABLE void cancelRemoval();
    Q_INVOKABLE void removeCandidate(const QString& key);
    Q_INVOKABLE void moveCandidate(const QString& key, int destination);

signals:
    void savedValuesChanged();

private:
    enum Role { KeyRole = Qt::UserRole + 1, DraftRole, PlaceholderRole, CanRemoveRole, ErrorRole };
    enum class Error { None, Invalid, Duplicate };
    struct Row {
        QString key;
        QString saved;
        QString draft;
        bool placeholder = false;
        Error error = Error::None;
    };
    Row makeRow(const QString& value, bool placeholder = false);
    int rowForKey(const QString& key) const;
    int savedCount() const { return savedCount_; }
    QString errorMessage(Error error) const;
    QString normalizeDraftInput(const QString& text) const;
    void notifyRemovalState();

    editor::CompletionCandidateGroup group_;
    QList<Row> rows_;
    quint64 nextKey_ = 0;
    int savedCount_ = 0;
    QString pendingRemoval_;
    bool halfWidthInputEnabled_ = true;
};
} // namespace miacode::ui
