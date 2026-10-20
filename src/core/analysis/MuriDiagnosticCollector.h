#pragma once

#include <QString>
#include <QVector>

#include "core/analysis/MuriTypes.h"             // MuriDiagnostic, MuriJudgeSpriteEvent, MuriKind, ...
#include "core/analysis/MuriAnalyzerModel.h"  // DiagnosticAnchor, JudgeableSimpleNote

struct TimelineNoteMarker;

namespace miacode::muri::detail {

class MuriDiagnosticCollector {
public:
    // Marker-anchored diagnostic (slide/wifi "too fast", ...).
    void addDiagnostic(
        MuriKind kind,
        MuriAlertLevel alertLevel,
        double second,
        const TimelineNoteMarker& marker,
        const QString& markerKey,
        const QString& detail,
        const DiagnosticAnchor& anchor = DiagnosticAnchor(),
        MuriDetailKind detailKind = MuriDetailKind::None,
        const MuriDetailArgs& detailArgs = MuriDetailArgs());

    // Simple-note-anchored diagnostic (tap-on-slide / slide-head-tap / overlap).
    void addSimpleNoteDiagnostic(
        MuriKind kind,
        MuriAlertLevel alertLevel,
        double second,
        const JudgeableSimpleNote& note,
        const QString& detail,
        const DiagnosticAnchor& anchor = DiagnosticAnchor(),
        MuriDetailKind detailKind = MuriDetailKind::None,
        const MuriDetailArgs& detailArgs = MuriDetailArgs());

    // Judge-sprite events. The simple-note variant is a no-op unless the note was
    // judged bad on a real pad; the slide variant returns false for unusable lanes.
    void addSimpleJudgeSpriteEvent(
        const JudgeableSimpleNote& note,
        MuriSimpleJudgeEffect simpleEffect = MuriSimpleJudgeEffect::Good);
    bool appendSlideJudgeSpriteEvent(const TimelineNoteMarker& marker, double judgeSecond);

    // Stable-sort both streams by time/source and collapse dense same-second
    // overlap diagnostics. Call once, after every stage has injected.
    void finalize();

    QVector<MuriDiagnostic> diagnostics;
    QVector<MuriJudgeSpriteEvent> judgeSpriteEvents;
};

}  // namespace miacode::muri::detail
