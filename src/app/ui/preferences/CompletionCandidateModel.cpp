#include "app/ui/preferences/CompletionCandidateModel.h"

#include "editor/SimaiTextEditPolicy.h"

#include <QCoreApplication>

namespace miacode::ui {

CompletionCandidateModel::CompletionCandidateModel(editor::CompletionCandidateGroup group, QObject* parent)
    : QAbstractListModel(parent), group_(group)
{
    const auto defaults = editor::defaultCompletionConfig();
    replaceValues(group == editor::CompletionCandidateGroup::Duration ? defaults.durations : defaults.divisions);
}

int CompletionCandidateModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

QHash<int, QByteArray> CompletionCandidateModel::roleNames() const
{
    return {{KeyRole, "candidateKey"}, {DraftRole, "draftValue"},
            {PlaceholderRole, "placeholderRow"}, {CanRemoveRole, "removable"},
            {ErrorRole, "validationMessage"}};
}

QVariant CompletionCandidateModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size()) return {};
    const Row& row = rows_.at(index.row());
    switch (role) {
    case KeyRole: return row.key;
    case DraftRole: return row.draft;
    case PlaceholderRole: return row.placeholder;
    case CanRemoveRole: return !row.placeholder && (row.saved.isEmpty() || savedCount() > 1);
    case ErrorRole: return errorMessage(row.error);
    default: return {};
    }
}

CompletionCandidateModel::Row CompletionCandidateModel::makeRow(const QString& value, bool placeholder)
{
    return {QString::number(++nextKey_), value, value, placeholder, Error::None};
}

int CompletionCandidateModel::rowForKey(const QString& key) const
{
    for (int i = 0; i < rows_.size(); ++i) if (rows_.at(i).key == key) return i;
    return -1;
}

QStringList CompletionCandidateModel::savedValues() const
{
    QStringList values;
    values.reserve(savedCount_);
    for (const Row& row : rows_) if (!row.saved.isEmpty()) values.append(row.saved);
    return values;
}

QString CompletionCandidateModel::normalizeDraftInput(const QString& text) const
{
    const QString normalized = halfWidthInputEnabled_ ? editor::normalizeSimaiInput(text) : text;
    return editor::sanitizeCompletionInput(normalized);
}

QString CompletionCandidateModel::normalizeInput(const QString& text) const
{
    return normalizeDraftInput(text);
}

void CompletionCandidateModel::setHalfWidthInputEnabled(bool enabled)
{
    if (halfWidthInputEnabled_ == enabled) return;
    halfWidthInputEnabled_ = enabled;

    bool draftChanged = false;
    for (Row& row : rows_) {
        const QString normalizedDraft = normalizeDraftInput(row.draft);
        if (row.draft != normalizedDraft) {
            row.draft = normalizedDraft;
            row.error = Error::None;
            draftChanged = true;
        }
    }
    if (draftChanged && rowCount() > 0) {
        emit dataChanged(index(0), index(rowCount() - 1), {DraftRole, ErrorRole});
    }
}

void CompletionCandidateModel::replaceValues(const QStringList& values, bool discardDrafts)
{
    QStringList normalizedInput;
    normalizedInput.reserve(values.size());
    for (const QString& value : values) normalizedInput.append(normalizeDraftInput(value));
    const QStringList normalized = editor::normalizeCompletionValues(group_, normalizedInput);
    if (!discardDrafts && !rows_.isEmpty() && savedValues() == normalized) return;
    beginResetModel();
    rows_.clear();
    savedCount_ = static_cast<int>(normalized.size());
    for (const QString& value : normalized) rows_.append(makeRow(value));
    rows_.append(makeRow({}, true));
    pendingRemoval_.clear();
    endResetModel();
}

void CompletionCandidateModel::setDraft(const QString& key, const QString& text)
{
    const int rowIndex = rowForKey(key);
    if (rowIndex < 0) return;
    rows_[rowIndex].draft = normalizeDraftInput(text);
    rows_[rowIndex].error = Error::None;
    const bool nonempty = !rows_[rowIndex].draft.isEmpty();
    if (rows_[rowIndex].placeholder && nonempty) {
        rows_[rowIndex].placeholder = false;
        emit dataChanged(index(rowIndex), index(rowIndex), {DraftRole, PlaceholderRole, CanRemoveRole, ErrorRole});
        const int last = rowCount();
        beginInsertRows({}, last, last);
        rows_.append(makeRow({}, true));
        endInsertRows();
    } else if (!rows_[rowIndex].placeholder && rows_[rowIndex].saved.isEmpty() && !nonempty) {
        // Keep this editor alive as the placeholder and remove the spare row.
        rows_[rowIndex].placeholder = true;
        const int last = rowCount() - 1;
        beginRemoveRows({}, last, last);
        rows_.removeAt(last);
        endRemoveRows();
        const int target = rowCount() - 1;
        if (rowIndex != target) {
            beginMoveRows({}, rowIndex, rowIndex, {}, target + 1);
            rows_.move(rowIndex, target);
            endMoveRows();
        }
        emit dataChanged(index(target), index(target), {DraftRole, PlaceholderRole, CanRemoveRole, ErrorRole});
    } else {
        emit dataChanged(index(rowIndex), index(rowIndex), {DraftRole, ErrorRole});
    }
}

QString CompletionCandidateModel::errorMessage(Error error) const
{
    if (error == Error::Duplicate) return qtTrId("preferences.completion.duplicate");
    if (error == Error::Invalid)
        return qtTrId(group_ == editor::CompletionCandidateGroup::Duration
            ? "preferences.completion.invalid_duration" : "preferences.completion.invalid_division");
    return {};
}

void CompletionCandidateModel::retranslateErrors()
{
    if (rowCount() > 0) emit dataChanged(index(0), index(rowCount() - 1), {ErrorRole});
}

void CompletionCandidateModel::notifyRemovalState()
{
    if (rowCount() > 0) emit dataChanged(index(0), index(rowCount() - 1), {CanRemoveRole});
}

bool CompletionCandidateModel::commit(const QString& key)
{
    const int rowIndex = rowForKey(key);
    if (rowIndex < 0 || key == pendingRemoval_) return false;
    Row& row = rows_[rowIndex];
    if (row.placeholder || (row.draft == row.saved && row.error == Error::None)) return true;
    const QString value = normalizeDraftInput(row.draft);
    row.draft = value;
    Error error = editor::isValidCompletionValue(group_, value) ? Error::None : Error::Invalid;
    if (error == Error::None) {
        for (const Row& other : rows_) {
            if (other.key != key && (other.saved == value || other.draft.trimmed() == value)) {
                error = Error::Duplicate;
                break;
            }
        }
    }
    row.error = error;
    const bool changed = error == Error::None && row.saved != value;
    if (error == Error::None) {
        if (row.saved.isEmpty()) ++savedCount_;
        row.saved = row.draft = value;
    }
    emit dataChanged(index(rowIndex), index(rowIndex), {DraftRole, ErrorRole});
    if (changed) { notifyRemovalState(); emit savedValuesChanged(); }
    return error == Error::None;
}

void CompletionCandidateModel::finishEditing()
{
    QStringList keys;
    for (const Row& row : rows_)
        if (!row.placeholder && (row.draft != row.saved || row.error != Error::None))
            keys.append(row.key);
    for (const QString& key : keys) {
        commit(key);
        const int rowIndex = rowForKey(key);
        if (rowIndex < 0) continue;
        if (rows_.at(rowIndex).saved.isEmpty()) {
            beginRemoveRows({}, rowIndex, rowIndex);
            rows_.removeAt(rowIndex);
            endRemoveRows();
        } else {
            rows_[rowIndex].draft = rows_.at(rowIndex).saved;
            rows_[rowIndex].error = Error::None;
            emit dataChanged(index(rowIndex), index(rowIndex), {DraftRole, ErrorRole});
        }
    }
    pendingRemoval_.clear();
    notifyRemovalState();
}

void CompletionCandidateModel::prepareRemoval(const QString& key) { pendingRemoval_ = key; }
void CompletionCandidateModel::cancelRemoval() { pendingRemoval_.clear(); }

void CompletionCandidateModel::removeCandidate(const QString& key)
{
    pendingRemoval_ = key;
    const int rowIndex = rowForKey(key);
    if (rowIndex < 0 || rows_.at(rowIndex).placeholder
        || (!rows_.at(rowIndex).saved.isEmpty() && savedCount() <= 1)) {
        pendingRemoval_.clear();
        return;
    }
    const bool persisted = !rows_.at(rowIndex).saved.isEmpty();
    if (persisted) --savedCount_;
    beginRemoveRows({}, rowIndex, rowIndex);
    rows_.removeAt(rowIndex);
    endRemoveRows();
    pendingRemoval_.clear();
    notifyRemovalState();
    if (persisted) emit savedValuesChanged();
}

void CompletionCandidateModel::moveCandidate(const QString& key, int destination)
{
    const int source = rowForKey(key);
    if (source < 0 || rows_.at(source).placeholder) return;
    const int target = qBound(0, destination, rowCount() - 2);
    if (source == target) return;
    const QStringList before = savedValues();
    beginMoveRows({}, source, source, {}, target > source ? target + 1 : target);
    rows_.move(source, target);
    endMoveRows();
    if (savedValues() != before) emit savedValuesChanged();
}
} // namespace miacode::ui
