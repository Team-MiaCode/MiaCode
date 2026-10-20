#pragma once

#include <QByteArray>
#include <QObject>
#include <QTimer>
#include <QVector>

#include <optional>

#include "app/services/ChartWorkspace.h"
#include "core/analysis/MuriRenderOptions.h"
#include "core/analysis/MuriTypes.h"
#include "common/TaskCancellation.h"
#include "core/chart/parser/SimaiParser.h"
#include "core/chart/model/TimelineData.h"
#include "core/analysis/TimelineSlowRefresh.h"

namespace miacode {

// Shared parse output. Preview consumers can use this before diagnostics finish.
struct ParsedChartSnapshot {
    quint64 revision = 0;
    quint64 documentOpenGeneration = 0;
    int difficultyId = 0;
    bool available = false;
    QString chartText;
    double firstSeconds = 0.0;
    miacode::simai::SimaiTimingMetadata timingMetadata;
    SimaiParseResult parseResult;
    TimelinePreviewRefreshState previewState;
};

// Revision-stamped diagnostics derived from the shared parse output.
struct AnalysisSnapshot {
    quint64 revision = 0;
    int difficultyId = 0;
    bool available = false;
    bool pending = false;
    SimaiValidationLocale locale = SimaiValidationLocale::English;
    SimaiValidationReport validation;
    QVector<TimelineNoteMarker> noteMarkers;
    QByteArray noteMarkerSignature;
    MuriAnalysisReport muri;
    QVector<MuriStaticReference> muriStaticReferences;
};

class AnalysisService final : public QObject
{
    Q_OBJECT

public:
    explicit AnalysisService(
        ChartWorkspace& workspace,
        SimaiValidationLocale locale = SimaiValidationLocale::English,
        const MuriRenderOptions& renderOptions = {},
        double staticTapOnSlideThresholdSeconds = -1.0,
        QObject* parent = nullptr);
    ~AnalysisService() override;

    AnalysisSnapshot snapshot() const;
    const ParsedChartSnapshot& parsedSnapshot() const { return parsedSnapshot_; }
    void requestAnalysis();
    void shutdown();
    void setDiagnosticsDeferred(bool deferred);
    void setLocale(SimaiValidationLocale locale);
    // The panel follows the preview's muri parameters. Re-analyzes only when one the
    // analyzer reads (hand radius, wifi C rule, tail threshold) actually moved, so a
    // render-mode or overlay toggle does not re-run the whole analysis.
    void setMuriParameters(const MuriRenderOptions& renderOptions,
                           double staticTapOnSlideThresholdSeconds);

    static AnalysisSnapshot analyze(
        const ChartWorkspace& workspace,
        SimaiValidationLocale locale = SimaiValidationLocale::English,
        const MuriRenderOptions& renderOptions = {},
        double staticTapOnSlideThresholdSeconds = -1.0);

signals:
    void parseReady(int difficultyId, quint64 revision);
    // The whole pending/available value is installed before this signal is
    // emitted. Consumers read it once and gate the complete package by the
    // workspace (difficultyId, revision) identity.
    void snapshotChanged(int difficultyId, quint64 revision);
    void analysisReady(int difficultyId, quint64 revision);

private:
    void cancelPendingAnalysis();
    static ParsedChartSnapshot parse(ParsedChartSnapshot request);
    static AnalysisSnapshot diagnose(const ParsedChartSnapshot& parsed,
                                    SimaiValidationLocale locale,
                                    const MuriRenderOptions& renderOptions,
                                    double staticTapOnSlideThresholdSeconds);
    void dispatchPendingParse();
    void requestDiagnostics();
    void dispatchDiagnostics();
    bool identityIsCurrent(int difficultyId, quint64 revision) const;

    ChartWorkspace* workspace_ = nullptr;
    SimaiValidationLocale locale_ = SimaiValidationLocale::English;
    MuriRenderOptions renderOptions_;
    double staticTapOnSlideThresholdSeconds_ = -1.0;
    ParsedChartSnapshot parsedSnapshot_;
    AnalysisSnapshot snapshot_;
    std::optional<ParsedChartSnapshot> pendingParse_;
    QTimer diagnosticsTimer_;
    miacode::task::CancellationFlag parseCancellation_;
    miacode::task::CancellationFlag diagnosticsCancellation_;
    quint64 diagnosticsGeneration_ = 0;
    bool parseWorkerRunning_ = false;
    bool diagnosticsWorkerRunning_ = false;
    bool diagnosticsRequested_ = false;
    bool diagnosticsDeferred_ = false;
    bool shuttingDown_ = false;
};

}  // namespace miacode
