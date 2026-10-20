#pragma once

#include <QHash>
#include <QString>
#include <QVector>

#include "core/analysis/MuriTypes.h"

struct TimelineNoteMarker;

namespace miacode::muri::detail {

// Appends a pad-press window and hand-action trail for every note marker. This
// is the report's playfield overlay; pure data → data. Trails other than wifi's
// are drawn at `handRadius`, the hand the runtime judge uses.
void buildOverlayActions(
    const QVector<TimelineNoteMarker>& noteMarkers,
    QVector<MuriPadWindow>* padWindows,
    QVector<MuriActionTrail>* actionTrails,
    double handRadius);

// Reconstructs the per-marker Muri state for a slide marker: each segment's area
// checkpoints are resolved against the pad windows in allWindows (indexed by pad
// in windowsByPad), and the marker is flagged early-cleared/flash when its last
// segment would land outside the critical window.
MarkerMuriState buildSlideState(
    const TimelineNoteMarker& marker,
    const QHash<QString, QVector<int>>& windowsByPad,
    const QVector<MuriPadWindow>& allWindows);

// Wifi counterpart of buildSlideState. Wifi traces have no pad-window
// dependency, so this resolves purely from the marker's own enter-time timeline.
MarkerMuriState buildWifiState(const TimelineNoteMarker& marker);

}  // namespace miacode::muri::detail
