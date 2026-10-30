#pragma once

#include <QObject>
#include <QAbstractListModel>
#include <QVariantList>

#include "app/services/AnalysisProjection.h"
#include "app/ui/document/AnalysisDecoration.h"
#include "app/services/AnalysisService.h"
#include "app/services/ChartWorkspace.h"

namespace miacode::ui {

class MuriRowsModel final : public QAbstractListModel
{
public:
    explicit MuriRowsModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void setSnapshot(const miacode::AnalysisSnapshot& snapshot);
    AnalysisRow row(int index) const;
    QVector<AnalysisDecoration> decorationRows() const;
    int warningCount() const { return warningCount_; }
    int issueCount() const { return rowCount() - warningCount_; }

private:
    miacode::AnalysisSnapshot snapshot_;
    int warningCount_ = 0;
    mutable bool decorationsReady_ = false;
    mutable QVector<AnalysisDecoration> decorationRows_;
};

class AnalysisModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList validationRows READ validationRows NOTIFY changed)
    Q_PROPERTY(QAbstractItemModel* muriRows READ muriRows CONSTANT)
    Q_PROPERTY(int muriRowCount READ muriRowCount NOTIFY changed)
    Q_PROPERTY(int muriWarningCount READ muriWarningCount NOTIFY changed)
    Q_PROPERTY(int muriIssueCount READ muriIssueCount NOTIFY changed)
    Q_PROPERTY(bool pending READ pending NOTIFY changed)
    Q_PROPERTY(bool validationPending READ validationPending NOTIFY changed)
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(int difficultyId READ difficultyId NOTIFY changed)
    Q_PROPERTY(qulonglong revision READ revision NOTIFY changed)
    Q_PROPERTY(int markerCount READ markerCount NOTIFY changed)

public:
    AnalysisModel(
        miacode::ChartWorkspace& workspace,
        miacode::AnalysisService& analysisService, QObject* parent = nullptr);

    QVariantList validationRows() const;
    QAbstractItemModel* muriRows() { return &muriRows_; }
    int muriRowCount() const { return muriRows_.rowCount(); }
    int muriWarningCount() const { return muriRows_.warningCount(); }
    int muriIssueCount() const { return muriRows_.issueCount(); }
    QVector<AnalysisDecoration> muriDecorationRows() const { return muriRows_.decorationRows(); }
    bool pending() const;
    bool validationPending() const;
    bool available() const;
    int difficultyId() const;
    qulonglong revision() const;
    int markerCount() const;
    void refreshPreferences();
    Q_INVOKABLE void activateRow(const QVariantMap& row);
    Q_INVOKABLE bool completeRowActivation(
        int difficultyId, qulonglong revision, int line, int column, int endColumn, double second);
    Q_INVOKABLE void cancelRowActivation(
        int difficultyId, qulonglong revision, int line, int column, int endColumn, double second);

signals:
    void changed();
    void rowActivated(int difficultyId, qulonglong revision, int line, int column, int endColumn, double second);

private:
    void refresh();
    bool rowIsCurrent(const AnalysisRow& row, int muriIndex) const;
    QVariantList rowsToVariantList(const QVector<miacode::ui::AnalysisRow>& rows) const;

    miacode::ChartWorkspace* workspace_ = nullptr;
    miacode::AnalysisService* analysisService_ = nullptr;
    MuriRowsModel muriRows_;
    int activationMuriIndex_ = -1;
    miacode::ui::AnalysisProjection projection_;
    miacode::ui::AnalysisActivationState activationState_;
};
} // namespace miacode::ui
