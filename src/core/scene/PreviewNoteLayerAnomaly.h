#pragma once

#include "core/chart/model/TimelineData.h"
#include "core/scene/PreviewFireworkWarmupPolicy.h"

#include <QVector>
#include <QtGlobal>

#include <cmath>

// Field diagnostic for issue #48: every note sprite vanished while the
// statistics fed from the session-side markers stayed correct. The scene root
// logs when its note cursors come up empty inside the chart, so a --debug
// capture can tell a stale prepared cache or a stuck cursor from an ordinary
// gap between notes.
namespace miacode::preview::scene {

// Chart time covered by the markers, without the firework warm-up synthetic.
struct PreviewNoteMarkerSpan {
    int noteCount = 0;
    double firstSecond = 0.0;
    double lastSecond = 0.0;
};

inline PreviewNoteMarkerSpan previewNoteMarkerSpan(const QVector<TimelineNoteMarker>& markers)
{
    PreviewNoteMarkerSpan span;
    for (const TimelineNoteMarker& marker : markers) {
        if (isFireworkWarmupMarker(marker)) {
            continue;
        }
        const double endSecond = qMax(marker.second, qMax(marker.endSecond, marker.slideTraceSecond));
        if (span.noteCount == 0) {
            span.firstSecond = marker.second;
            span.lastSecond = endSecond;
        } else {
            span.firstSecond = qMin(span.firstSecond, marker.second);
            span.lastSecond = qMax(span.lastSecond, endSecond);
        }
        ++span.noteCount;
    }
    return span;
}

// A long rest inside the chart also matches, so this marks a candidate for the
// log, not a confirmed fault.
inline bool previewNoteLayersEmptyAnomaly(
    const PreviewNoteMarkerSpan& span,
    double playheadSeconds,
    qsizetype headActiveCount,
    qsizetype trackActiveCount,
    qsizetype touchActiveCount)
{
    return span.noteCount > 0
        && headActiveCount == 0
        && trackActiveCount == 0
        && touchActiveCount == 0
        && std::isfinite(playheadSeconds)
        && playheadSeconds >= span.firstSecond
        && playheadSeconds <= span.lastSecond;
}

}  // namespace miacode::preview::scene
