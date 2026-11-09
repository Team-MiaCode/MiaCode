#include "core/chart/transform/ChartNoteSelection.h"

#include "core/chart/parser/SimaiCommentScan.h"
#include "core/chart/parser/SimaiParser.h"
#include "core/chart/document/SimaiTimingMetadata.h"

#include <QRegularExpression>
#include <algorithm>
#include <cmath>

namespace miacode::chart_selection {
namespace {
bool intersectsSelection(const NoteRange& note, const NoteRanges& selection)
{
    const auto first = std::lower_bound(selection.cbegin(), selection.cend(), note.start,
        [](const NoteRange& range, int start) { return range.end <= start; });
    return first != selection.cend() && first->start < note.end;
}

NoteTimingState timingAt(const QString& text, int position, NoteTimingState state)
{
    // Ignore the contents of note durations, even if the surrounding draft
    // is unfinished. Timing state only comes from complete directives.
    static const QRegularExpression directives(
        QStringLiteral(R"(\[[^\]]*\]|\(([^()]*)\)|\{([^{}]*)\}|<HS\*([^>]*)>)"));
    for (int lineStart = 0; lineStart < position;) {
        int lineEnd = text.indexOf(QLatin1Char('\n'), lineStart);
        if (lineEnd < 0) lineEnd = text.size();
        const QString line = text.mid(lineStart, qMin(position, lineEnd) - lineStart);
        const int comment = line.indexOf(QStringLiteral("||"));
        auto matches = directives.globalMatch(comment < 0 ? line : line.left(comment));
        while (matches.hasNext()) {
            const auto directive = matches.next();
            bool ok = false;
            if (directive.capturedStart(1) >= 0) {
                const double bpm = directive.captured(1).toDouble(&ok);
                if (ok && std::isfinite(bpm) && bpm > 0.0) state.bpm = directive.captured();
            } else if (directive.capturedStart(2) >= 0) {
                const int beats = directive.captured(2).toInt(&ok);
                if (ok && beats > 0) state.subdivision = directive.captured();
            } else if (directive.capturedStart(3) >= 0) {
                const double hs = directive.captured(3).toDouble(&ok);
                if (ok && std::isfinite(hs) && (hs > 0.0 || (hs < 0.0 && SimaiParser::allowNegativeHsEnabled())))
                    state.hs = directive.captured();
            }
        }
        if (comment >= 0) {
            miacode::simai::parseInlineTimeSignatureComment(
                line, comment, &state.meterNumerator, &state.meterDenominator);
        }
        lineStart = lineEnd + 1;
    }
    return state;
}
QString timingText(const NoteTimingState& state)
{
    return QStringLiteral("|| %1/%2\n").arg(state.meterNumerator).arg(state.meterDenominator)
        + state.bpm + state.subdivision + state.hs;
}

// Repair only note separators whose neighbour was removed. Timing commas,
// directives, comments and whitespace retain their original source positions.
QString withoutNotes(const QString& text, const NoteRanges& removed, const NoteRanges& allNotes)
{
    QByteArray erase(text.size(), 0);
    for (const auto& range : removed)
        for (int i = range.start; i < range.end; ++i) erase[i] = 1;
    QByteArray content(text.size(), 0);
    for (const auto& span : miacode::simai::chartContentSpans(text, 0, text.size()))
        for (int i = span.start; i < span.end; ++i) content[i] = 1;
    QByteArray noteContent(text.size(), 0);
    for (const auto& note : allNotes)
        for (int i = note.start; i < note.end; ++i) noteContent[i] = 1;

    // A run of adjacent notes/separators is one simultaneous-note group. Keep
    // one original separator between surviving tokens; strip leading/trailing
    // separators. Packed lanes need no added separators.
    int groupStart = 0;
    int nextNote = 0;
    for (int i = 0; i <= text.size(); ++i) {
        if (i != text.size() && (content[i] == 0 || (text[i] != QLatin1Char(',')
            && text[i] != QLatin1Char('\n')))) continue;
        QVector<NoteRange> notes;
        bool changed = false;
        while (nextNote < allNotes.size() && allNotes[nextNote].start < i) {
            const auto& note = allNotes[nextNote++];
            if (note.start < groupStart) continue;
            if (erase[note.start]) changed = true;
            else notes.append(note);
        }
        if (changed) {
            for (int j = groupStart; j < i; ++j) {
                if (!content[j] || noteContent[j]) continue;
                const QChar ch = text[j];
                if (ch == QLatin1Char('/') || ch == QLatin1Char('`')) erase[j] = 1;
            }
            for (int n = 1; n < notes.size(); ++n) {
                int separator = -1;
                for (int j = notes[n - 1].end; j < notes[n].start; ++j) {
                    if (!content[j] || noteContent[j]) continue;
                    if (text[j] == QLatin1Char('`')) { separator = j; break; }
                    if (text[j] == QLatin1Char('/') && separator < 0) separator = j;
                }
                if (separator >= 0) erase[separator] = 0;
            }
        }
        groupStart = i + 1;
    }
    QString result;
    result.reserve(text.size());
    for (int i = 0; i < text.size(); ++i) if (!erase[i]) result.append(text[i]);
    return result;
}
}

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

QString deleteSelectedNotes(const QString& text, const NoteRanges& selected, const NoteRanges& allNotes)
{
    const auto selection = normalizedNoteRanges(selected, text.size());
    if (selection.isEmpty()) return text;
    const auto notes = normalizedNoteRanges(allNotes, text.size());
    NoteRanges removed;
    for (const auto& note : notes)
        if (intersectsSelection(note, selection)) removed.append(note);
    return removed.isEmpty() ? text : withoutNotes(text, removed, notes);
}

QString copySelectedNotes(const QString& text, const NoteRanges& selected, const NoteRanges& allNotes,
    const NoteTimingState& initialTiming)
{
    const auto selection = normalizedNoteRanges(selected, text.size());
    const auto notes = normalizedNoteRanges(allNotes, text.size());
    NoteRanges kept, removed;
    for (const auto& note : notes) {
        if (intersectsSelection(note, selection)) kept.append(note);
        else removed.append(note);
    }
    if (kept.isEmpty()) return {};
    const int start = kept.first().start, end = kept.last().end;
    // Restore the timing state in force at the first selected note. Directives
    // inside the copied interval, including BPM changes and empty beats, stay.
    const QString prefix = timingText(timingAt(text, start, initialTiming));
    NoteRanges localNotes, localRemoved;
    for (const auto& note : notes) if (note.start >= start && note.end <= end)
        localNotes.append({note.start - start, note.end - start});
    for (const auto& note : removed) if (note.start >= start && note.end <= end)
        localRemoved.append({note.start - start, note.end - start});
    // A clipboard fragment deliberately has no terminal E: ordinary text paste
    // in another editor must not terminate the recipient chart.
    return prefix + withoutNotes(text.mid(start, end - start), localRemoved, localNotes) + QStringLiteral(",\n");
}

NotePasteResult pasteNoteFragment(const QString& text, int start, int end,
    const QString& input, const NoteRanges& selectedNotes, const NoteRanges& allNotes,
    const NoteTimingState& initialTiming)
{
    NotePasteResult result{text, qBound(0, start, int(text.size()))};
    QString fragment = input.trimmed();
    static const QRegularExpression terminal(
        QStringLiteral(R"((?:^|[,\n])[ \t]*([Ee])[ \t]*(?:\|\|[^\n]*)?\s*\z)"));
    const auto fragmentEnd = terminal.match(fragment);
    if (fragmentEnd.hasMatch()) fragment.truncate(fragmentEnd.capturedStart(1));
    fragment = fragment.trimmed();
    if (fragment.isEmpty()) return result;
    const auto parsedFragment = SimaiParser::parseForTimeline(fragment);
    if (!parsedFragment.ok || parsedFragment.noteMarkers.isEmpty()) return result;
    auto selected = normalizedNoteRanges(selectedNotes, text.size());
    int at = qBound(0, qMin(start, end), int(text.size()));
    int limit = qBound(at, qMax(start, end), int(text.size()));
    if (selected.isEmpty() && limit > at) {
        for (const auto& note : allNotes)
            if (note.start < limit && at < note.end) selected.append(note);
    }
    QString base = text;
    QString leading;
    if (!selected.isEmpty()) {
        // One paste for the whole collection. All selected objects are removed
        // together; the fragment is inserted once before its earliest beat.
        at = miacode::simai::previousChartComma(text, selected.first().start) + 1;
        base = deleteSelectedNotes(text, selected, allNotes);
    } else {
        const auto targetEnd = terminal.match(text);
        if (targetEnd.hasMatch()) at = qMin(at, int(targetEnd.capturedStart(1)));
        const int comment = miacode::simai::commentStartForPosition(text, at);
        if (comment >= 0) at = comment;
        for (const auto& note : allNotes) {
            if (note.start < at && at < note.end) { at = note.start; break; }
        }
        // Pasting inside a directive or a simultaneous separator goes before
        // that beat, preserving every original token and separator verbatim.
        const int beatStart = miacode::simai::previousChartComma(text, at) + 1;
        const QString prefix = text.mid(beatStart, at - beatStart).trimmed();
        const QChar before = prefix.isEmpty() ? QChar() : prefix.back();
        const QChar after = at < text.size() ? text[at] : QChar();
        bool insideDirective = false;
        for (int i = beatStart; i < at; ++i) {
            QChar close;
            if (text[i] == QLatin1Char('(')) close = QLatin1Char(')');
            else if (text[i] == QLatin1Char('{')) close = QLatin1Char('}');
            else if (text.mid(i, 4) == QLatin1String("<HS*")) close = QLatin1Char('>');
            if (close.isNull()) continue;
            const int closing = text.indexOf(close, i + 1);
            if (closing >= at) { insideDirective = true; break; }
            if (closing >= 0) i = closing;
        }
        if (insideDirective || before == QLatin1Char('/') || before == QLatin1Char('`')
            || after == QLatin1Char('/') || after == QLatin1Char('`')) at = beatStart;
        const bool afterNote = std::any_of(allNotes.cbegin(), allNotes.cend(), [&](const NoteRange& note) {
            return note.start >= beatStart && note.start < at;
        });
        if (afterNote) {
            const int comma = miacode::simai::nextChartComma(text, at);
            const bool sameBeatNote = std::any_of(allNotes.cbegin(), allNotes.cend(), [&](const NoteRange& note) {
                return note.start >= at && (comma < 0 || note.start < comma);
            });
            if (sameBeatNote) at = beatStart;
            // Preserve the original separator even across whitespace and comments.
            else if (comma >= 0) at = comma + 1;
            else leading = QStringLiteral(",");
        }
    }
    const auto restore = timingAt(text, at, initialTiming);
    const QString inserted = leading + fragment + (fragment.endsWith(QLatin1Char(',')) ? QString() : QStringLiteral(","))
        + QStringLiteral("\n") + timingText(restore);
    base.insert(at, inserted);
    result.text = base;
    result.caret = at + inserted.size();
    return result;
}
} // namespace miacode::chart_selection
