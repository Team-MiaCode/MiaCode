#include "core/chart/transform/ChartNoteSelection.h"

#include <algorithm>

namespace miacode::chart_selection {

NoteRanges normalizedNoteRanges(NoteRanges ranges, int textLength)
{
    for (auto& range : ranges) {
        range.start = qBound(0, range.start, textLength);
        range.end = qBound(range.start, range.end, textLength);
    }
    ranges.removeIf([](const NoteRange& range) { return range.end <= range.start; });
    std::sort(ranges.begin(), ranges.end(), [](const NoteRange& a, const NoteRange& b) {
        return a.start < b.start || (a.start == b.start && a.end < b.end);
    });
    NoteRanges result;
    for (const auto& range : ranges) {
        // Do not merge adjacent packed notes: they remain independent objects.
        if (!result.isEmpty() && range.start < result.last().end)
            result.last().end = qMax(result.last().end, range.end);
        else result.append(range);
    }
    return result;
}

} // namespace miacode::chart_selection
