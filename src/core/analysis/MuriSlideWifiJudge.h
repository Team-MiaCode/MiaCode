#pragma once

#include <QHash>
#include <QString>
#include <QVector>

#include "core/analysis/MuriRenderOptions.h"
#include "core/analysis/MuriTypes.h"
#include "core/analysis/MuriAnalyzerModel.h"

struct TimelineNoteMarker;

namespace miacode::muri::detail {

// Replays every slide / wifi marker against the full hand-action footprint and
// returns, per marker key, whether/when it was judged and the per-area hit trail.
QHash<QString, RuntimeSlideJudgeResult> simulateRuntimeSlideAndWifiJudgments(
    const QVector<TimelineNoteMarker>& noteMarkers,
    const QVector<JudgeableSimpleNote>& notes,
    const QVector<RuntimeTouchGroup>& touchGroups,
    const QHash<int, int>& touchGroupByChildNoteIndex,
    const MuriRenderOptions& renderOptions);

// Folds a simulation result back into the marker's overlay-built MarkerMuriState
// (per-area checkpoint causes + early-clear flash).
void applyRuntimeJudgeResultToState(const RuntimeSlideJudgeResult& result, MarkerMuriState* state);

// Source anchor for a slide-too-fast diagnostic: the foreign marker that
// early-judged this trace if known, else the trace's own marker.
DiagnosticAnchor diagnosticAnchorForSlideJudge(
    const TimelineNoteMarker& marker,
    const MarkerMuriState& state,
    const QHash<QString, MarkerSourceRef>& markerRefs,
    const QHash<QString, QString>& syntheticSlideHeadOwnerKeys);

// Human-readable detail line for a slide-too-fast diagnostic (target label,
// early-judge cause, and the timing gap in ms).
QString formatSlideTooFastDetail(
    const TimelineNoteMarker& marker,
    const MarkerMuriState& state,
    const QHash<QString, QString>& markerConfigLabels,
    const QHash<QString, QString>& syntheticSlideHeadOwnerKeys);
MuriDetailArgs slideTooFastDetailArgs(
    const TimelineNoteMarker& marker,
    const MarkerMuriState& state,
    const QHash<QString, QString>& markerConfigLabels,
    const QHash<QString, QString>& syntheticSlideHeadOwnerKeys,
    MuriDetailKind* detailKind);

}  // namespace miacode::muri::detail
