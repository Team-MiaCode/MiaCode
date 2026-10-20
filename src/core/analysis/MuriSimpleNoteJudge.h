#pragma once

#include <QVector>

#include "core/analysis/MuriRenderOptions.h"
#include "core/analysis/MuriDiagnosticCollector.h"

struct TimelineNoteMarker;

namespace miacode::muri::detail {

// Builds, judges, and reports the simple-note (tap/hold/touch) layer for one
// chart, writing tap-on-slide / overlap / multi-touch diagnostics and the
// simple-note judge sprite events into the collector. staticTapOnSlideThresholdSeconds
// is the user-tunable tap-on-slide warning cutoff.
void collectSimpleNoteRuntimeDiagnostics(
    const QVector<TimelineNoteMarker>& noteMarkers,
    const MuriRenderOptions& renderOptions,
    MuriDiagnosticCollector& collector,
    double staticTapOnSlideThresholdSeconds);

}  // namespace miacode::muri::detail
