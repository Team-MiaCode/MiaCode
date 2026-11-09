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

NoteRanges normalizedNoteRanges(NoteRanges ranges, int textLength);

} // namespace miacode::chart_selection
