#include "app/ui/document/AnalysisModel.h"

#include "core/analysis/MuriTypes.h"
#include "core/analysis/MuriPanelEntries.h"

#include <QVariantMap>
#include <QSet>

#include <utility>
#include <QCoreApplication>


namespace miacode::ui {
namespace {

QVariantMap rowToVariantMap(const AnalysisRow& row)
{
    return {
        {QStringLiteral("line"), row.line},
        {QStringLiteral("column"), row.column},
        {QStringLiteral("endColumn"), row.endColumn},
        {QStringLiteral("second"), row.second},
        {QStringLiteral("severity"), row.severity},
        {QStringLiteral("alert"), row.alert},
        {QStringLiteral("code"), row.code},
        {QStringLiteral("title"), row.title},
        {QStringLiteral("detail"), row.detail},
        {QStringLiteral("difficultyId"), row.difficultyId},
        {QStringLiteral("revision"), QVariant::fromValue<qulonglong>(row.revision)},
    };
}

} // namespace

int MuriRowsModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : snapshot_.muriPanelEntries.size();
}

QHash<int, QByteArray> MuriRowsModel::roleNames() const
{
    return {{Qt::UserRole, QByteArrayLiteral("modelData")}};
}

QVariant MuriRowsModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount() || role != Qt::UserRole) return {};
    auto result = rowToVariantMap(row(index.row()));
    result.insert(QStringLiteral("rowIndex"), index.row());
    return result;
}

AnalysisRow MuriRowsModel::row(int index) const
{
    const auto entry = miacode::muri::resolveMuriPanelEntry(
        snapshot_.muriPanelEntries.at(index), snapshot_.muri, snapshot_.muriStaticReferences);
    AnalysisRow row;
    row.line = qMax(1, entry.line);
    row.column = qMax(1, entry.col);
    row.endColumn = row.column;
    row.second = entry.second;
    const bool warning = entry.alertLevel == MuriAlertLevel::Warning;
    row.severity = warning ? QStringLiteral("warning") : QStringLiteral("error");
    row.alert = warning ? QStringLiteral("warning") : QStringLiteral("muri");
    row.title = muriKindText(entry.kind);
    row.detail = renderMuriDetail(entry.detailKind, entry.detailArgs, snapshot_.locale).trimmed();
    if (row.detail.isEmpty()) row.detail = entry.rawDetail;
    row.difficultyId = snapshot_.difficultyId;
    row.revision = snapshot_.revision;
    return row;
}

void MuriRowsModel::setSnapshot(const miacode::AnalysisSnapshot& snapshot)
{
    if (snapshot_.revision == snapshot.revision && snapshot_.difficultyId == snapshot.difficultyId
        && snapshot_.locale == snapshot.locale
        && snapshot_.muriPanelEntries.constData() == snapshot.muriPanelEntries.constData()) return;
    beginResetModel();
    snapshot_ = snapshot;
    warningCount_ = 0;
    for (const auto& source : std::as_const(snapshot_.muriPanelEntries)) {
        if (source.alertLevel == MuriAlertLevel::Warning) ++warningCount_;
    }
    decorationRows_.clear();
    decorationsReady_ = false;
    endResetModel();
}

QVector<AnalysisDecoration> MuriRowsModel::decorationRows() const
{
    if (decorationsReady_) return decorationRows_;
    QSet<QPair<quint64, bool>> locations;
    for (const auto& source : std::as_const(snapshot_.muriPanelEntries)) {
        const int line = qMax(1, source.line);
        const int column = qMax(1, source.col);
        const bool warning = source.alertLevel == MuriAlertLevel::Warning;
        const auto key = qMakePair((quint64(line) << 32) | quint32(column), warning);
        if (locations.contains(key)) continue;
        locations.insert(key);
        decorationRows_.append({line, column, column, warning});
    }
    decorationsReady_ = true;
    return decorationRows_;
}

AnalysisModel::AnalysisModel(
    miacode::ChartWorkspace& workspace,
    miacode::AnalysisService& analysisService, QObject* parent)
    : QObject(parent)
    , workspace_(&workspace)
    , analysisService_(&analysisService)
    , muriRows_(this)
{
    refresh();
    connect(analysisService_, &miacode::AnalysisService::snapshotChanged,
            this, [this](int, quint64) {
        refresh();
        emit changed();
    });
}

QVariantList AnalysisModel::validationRows() const { return rowsToVariantList(projection_.validationRows); }
bool AnalysisModel::pending() const { return projection_.pending; }
bool AnalysisModel::validationPending() const { return projection_.validationPending; }
bool AnalysisModel::available() const { return projection_.available; }
int AnalysisModel::difficultyId() const { return projection_.difficultyId; }
qulonglong AnalysisModel::revision() const { return projection_.revision; }
int AnalysisModel::markerCount() const { return projection_.noteMarkers.size(); }

void AnalysisModel::refreshPreferences()
{
    refresh();
    emit changed();
}

void AnalysisModel::activateRow(const QVariantMap& row)
{
    const int difficultyId = row.value(QStringLiteral("difficultyId")).toInt();
    const quint64 revision = row.value(QStringLiteral("revision")).toULongLong();
    const int line = row.value(QStringLiteral("line")).toInt();
    const int column = row.value(QStringLiteral("column")).toInt();
    const int endColumn = row.value(QStringLiteral("endColumn"), column).toInt();
    const double second = row.value(QStringLiteral("second"), -1.0).toDouble();
    miacode::ui::AnalysisRow candidate;
    candidate.line = line;
    candidate.column = column;
    candidate.endColumn = endColumn;
    candidate.second = second;
    candidate.difficultyId = difficultyId;
    candidate.revision = revision;
    candidate.title = row.value(QStringLiteral("title")).toString();
    candidate.detail = row.value(QStringLiteral("detail")).toString();
    const int muriIndex = row.value(QStringLiteral("rowIndex"), -1).toInt();
    if (!rowIsCurrent(candidate, muriIndex)) return;
    activationMuriIndex_ = muriIndex;
    activationState_.begin(candidate);
    emit rowActivated(
        difficultyId, revision, qMax(1, line), qMax(1, column), qMax(column, endColumn), second);
}

bool AnalysisModel::completeRowActivation(
    int difficultyId, qulonglong revision, int line, int column, int endColumn, double second)
{
    miacode::ui::AnalysisRow completionIdentity;
    completionIdentity.difficultyId = difficultyId;
    completionIdentity.revision = revision;
    completionIdentity.line = line;
    completionIdentity.column = column;
    completionIdentity.endColumn = endColumn;
    completionIdentity.second = second;
    miacode::ui::AnalysisRow pending;
    if (!activationState_.complete(completionIdentity, &pending)) return false;
    if (!rowIsCurrent(pending, activationMuriIndex_)) return false;
    return true;
}

void AnalysisModel::cancelRowActivation(
    int difficultyId, qulonglong revision, int line, int column, int endColumn, double second)
{
    miacode::ui::AnalysisRow pending;
    pending.difficultyId = difficultyId;
    pending.revision = revision;
    pending.line = line;
    pending.column = column;
    pending.endColumn = endColumn;
    pending.second = second;
    activationState_.cancel(pending);
}

bool AnalysisModel::rowIsCurrent(const AnalysisRow& candidate, int muriIndex) const
{
    if (muriIndex < 0) return miacode::ui::analysisRowCanActivate(projection_, candidate, projection_.difficultyId);
    if (muriIndex >= muriRows_.rowCount() || projection_.pending) return false;
    const auto current = muriRows_.row(muriIndex);
    return current.difficultyId == candidate.difficultyId && current.revision == candidate.revision
        && current.line == candidate.line && current.column == candidate.column
        && current.endColumn == candidate.endColumn && current.second == candidate.second
        && current.title == candidate.title && current.detail == candidate.detail;
}

void AnalysisModel::refresh()
{
    const miacode::ChartWorkspaceSnapshot workspaceSnapshot = workspace_ != nullptr
        ? workspace_->snapshot() : miacode::ChartWorkspaceSnapshot();
    const miacode::AnalysisSnapshot analysisSnapshot = analysisService_ != nullptr
        ? analysisService_->snapshot() : miacode::AnalysisSnapshot();
    const bool current = analysisSnapshot.available && !analysisSnapshot.pending
        && analysisSnapshot.difficultyId == workspaceSnapshot.activeDifficultyId
        && analysisSnapshot.revision == workspaceSnapshot.revision;
    projection_ = miacode::ui::projectAnalysis(
        analysisSnapshot, workspaceSnapshot.activeDifficultyId, workspaceSnapshot.revision,
        {});
    muriRows_.setSnapshot(current ? analysisSnapshot : miacode::AnalysisSnapshot());
    if (projection_.available && workspace_ != nullptr) {
        const SimaiDifficultyData* difficulty =
            workspace_->document().difficulty(workspaceSnapshot.activeDifficultyId);
        if (difficulty != nullptr && difficulty->level.trimmed().isEmpty()) {
            miacode::ui::AnalysisRow row;
            row.line = 0;
            row.column = 0;
            row.endColumn = 0;
            row.severity = QStringLiteral("warning");
            row.alert = QStringLiteral("metadata");
            row.code = QStringLiteral("missing_difficulty_level");
            row.title = qtTrId("validation.difficulty_level_missing_type");
            row.detail = qtTrId("validation.difficulty_level_missing");
            row.difficultyId = workspaceSnapshot.activeDifficultyId;
            row.revision = workspaceSnapshot.revision;
            projection_.validationRows.append(std::move(row));
        }
    }
    if (activationState_.hasPending() && !rowIsCurrent(activationState_.pending(), activationMuriIndex_)) {
        activationState_.cancel(activationState_.pending());
    }
}

QVariantList AnalysisModel::rowsToVariantList(
    const QVector<miacode::ui::AnalysisRow>& rows) const
{
    QVariantList result;
    result.reserve(rows.size());
    for (const auto& row : rows) {
        result.append(rowToVariantMap(row));
    }
    return result;
}

} // namespace miacode::ui
