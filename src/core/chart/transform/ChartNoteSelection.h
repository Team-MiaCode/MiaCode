#pragma once

#include <QString>
#include <QVector>

namespace miacode::chart_selection {

// Half-open UTF-16 spans of independent notes. A chained/branched Slide owns
// its complete source token; packed taps own one span per lane.
struct NoteRange {
    int start = 0;
    int end = 0;
    bool operator==(const NoteRange&) const = default;
};
using NoteRanges = QVector<NoteRange>;
struct NoteTimingState {
    QString bpm = QStringLiteral("(120)");
    QString subdivision = QStringLiteral("{4}");
    QString hs = QStringLiteral("<HS*1>");
    int meterNumerator = 4;
    int meterDenominator = 4;
};
struct NotePasteResult {
    QString text;
    int caret = 0;
};

NoteRanges normalizedNoteRanges(NoteRanges ranges, int textLength);
QString deleteSelectedNotes(const QString& text, const NoteRanges& selected,
    const NoteRanges& allNotes);
QString copySelectedNotes(const QString& text, const NoteRanges& selected,
    const NoteRanges& allNotes, const NoteTimingState& initialTiming = {});
NotePasteResult pasteNoteFragment(const QString& text, int start, int end,
    const QString& fragment, const NoteRanges& selectedNotes, const NoteRanges& allNotes,
    const NoteTimingState& initialTiming = {});

} // namespace miacode::chart_selection
