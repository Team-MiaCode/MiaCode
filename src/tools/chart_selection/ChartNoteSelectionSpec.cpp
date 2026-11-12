#include "core/chart/transform/ChartNoteSelection.h"
#include "core/chart/parser/SimaiParser.h"
#include "timeline/TimelineQuickModel.h"

#include <QCoreApplication>
#include <QDebug>

using namespace miacode::chart_selection;

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    int failures = 0;
    const auto expect = [&](bool condition, const char* label) {
        if (!condition) { ++failures; qWarning() << "FAIL:" << label; }
    };
    const QString text = QStringLiteral("(150){8}<HS*2>1/2/3,4,,(240){16}5h[4:2]/A1,1-5-3[4:4], || 注释,/7\nE");
    TimelineQuickModel model;
    model.rebuildFromText(text, 0.0);
    const auto all = model.noteSourceRanges();
    const auto range = [&](const QString& token) {
        const int at = text.indexOf(token);
        return NoteRange{at, at + int(token.size())};
    };
    const auto first = range(QStringLiteral("1/2/3"));
    expect(deleteSelectedNotes(text, {{first.start + 2, first.start + 3}}, all)
        == QString(text).replace(first.start, 5, QStringLiteral("1/3")), "remove middle simultaneous operand and repair slash");
    expect(deleteSelectedNotes(text, {{first.start, first.start + 1}}, all)
        == QString(text).replace(first.start, 5, QStringLiteral("2/3")), "remove first operand without a leading slash");
    expect(deleteSelectedNotes(text, {{first.start + 4, first.start + 5}}, all)
        == QString(text).replace(first.start, 5, QStringLiteral("1/2")), "remove last operand without a trailing slash");
    const auto hold = range(QStringLiteral("5h[4:2]"));
    const auto chain = range(QStringLiteral("1-5-3[4:4]"));
    const NoteRanges selected{{first.start + 2, first.start + 3}, hold, chain};
    const auto resolved = model.resolveNoteSelection(selected);
    expect(resolved.locations.size() == 3, "noncontiguous collection excludes intervening notes");
    expect(model.noteSourceRanges(&resolved.locations) == selected, "source locations round trip complete Slide tokens");
    const QString copied = copySelectedNotes(text, selected, all);
    const auto parsedCopy = SimaiParser::validateSyntax(copied + QStringLiteral("E"));
    expect(parsedCopy.ok && parsedCopy.noteMarkers.size() == 3, "clipboard is valid standalone Simai containing only selected notes");
    expect(copied.contains(QStringLiteral(",,,(240){16}")) && copied.startsWith(QStringLiteral("|| 4/4\n(150){8}<HS*2>2")),
        "clipboard retains empty beats, tempo and inherited subdivision/HS");
    const auto original = SimaiParser::parseForTimeline(text);
    expect(parsedCopy.noteMarkers.size() == 3 && original.noteMarkers.size() >= 7
        && qAbs(parsedCopy.noteMarkers[1].second - original.noteMarkers[4].second) < 1e-7
        && qAbs(parsedCopy.noteMarkers[2].second - original.noteMarkers.last().second) < 1e-7,
        "copied selected notes retain relative hit times across unselected notes and tempo changes");
    const auto cut = deleteSelectedNotes(text, selected, all);
    const auto parsedCut = SimaiParser::validateSyntax(cut);
    expect(parsedCut.ok && parsedCut.noteMarkers.size() == original.noteMarkers.size() - 3,
        "cut removes only the selected objects and remains valid");
    expect(cut.count(QLatin1Char(',')) == text.count(QLatin1Char(',')) && cut.contains(QStringLiteral("|| 注释,/7")),
        "cut preserves every timing comma and comment");
    for (const auto& note : parsedCut.noteMarkers) {
        bool same = false;
        for (const auto& before : original.noteMarkers)
            if (before.type == note.type && before.lane == note.lane && before.touchPad == note.touchPad
                && qAbs(before.second - note.second) < 1e-7) same = true;
        expect(same, "cut never moves a surviving note in time");
    }
    const QString split = QStringLiteral("(120){4}1/2`3,\nE");
    TimelineQuickModel splitModel;
    splitModel.rebuildFromText(split, 0.0);
    const int middle = split.indexOf(QLatin1Char('2'), 9);
    expect(deleteSelectedNotes(split, {{middle, middle + 1}}, splitModel.noteSourceRanges())
        == QStringLiteral("(120){4}1`3,\nE"), "separator repair preserves the original each-group boundary");
    const QString packed = QStringLiteral("(120){4}123,\nE");
    TimelineQuickModel packedModel;
    packedModel.rebuildFromText(packed, 0.0);
    expect(deleteSelectedNotes(packed, {{9, 10}}, packedModel.noteSourceRanges())
        == QStringLiteral("(120){4}13,\nE"), "packed lanes remain independent during deletion");
    expect(copySelectedNotes(text, {}, all).isEmpty() && deleteSelectedNotes(text, {}, all) == text,
        "an empty collection has no clipboard or document effect");
    for (const QString& branch : {QStringLiteral(""), QStringLiteral("-")}) {
        const QString draft = QStringLiteral("(120){4}1-5[8:1]*") + branch + QStringLiteral(",E");
        TimelineQuickModel draftModel;
        draftModel.rebuildFromText(draft, 0.0);
        const auto draftNotes = draftModel.noteSourceRanges();
        expect(draftNotes == NoteRanges{{8, int(draft.indexOf(QLatin1Char(',')))}}
            && deleteSelectedNotes(draft, draftNotes, draftNotes) == QStringLiteral("(120){4},E"),
            "visible branches in an unfinished draft retain the complete source token");
    }
    const QString target = QStringLiteral("(120){4}<HS*3>6,|| 3/8\n7,8,\nE");
    TimelineQuickModel targetModel;
    targetModel.rebuildFromText(target, 0);
    const int caret = target.indexOf(QStringLiteral("7,"));
    const auto pasted = pasteNoteFragment(target, caret, caret, copied, {}, targetModel.noteSourceRanges());
    const auto parsedPaste = SimaiParser::validateSyntax(pasted.text);
    expect(parsedPaste.ok && parsedPaste.noteMarkers.size() == 6,
        "a fragment pastes into a nonempty target without terminal E corruption");
    expect(pasted.text.endsWith(target.mid(caret)) && pasted.text.startsWith(target.left(caret)),
        "normal paste preserves every original character outside its insertion");
    if (parsedPaste.noteMarkers.size() == 6) {
        const auto& seven = parsedPaste.noteMarkers[4];
        const auto& eight = parsedPaste.noteMarkers[5];
        expect(seven.lane == 7 && eight.lane == 8 && seven.hsMultiplier == 3 && eight.hsMultiplier == 3
            && qAbs(eight.second - seven.second - 0.5) < 1e-7,
            "paste restores target BPM subdivision and HS for following notes");
        expect(pasted.text.contains(QStringLiteral("|| 3/8\n(120){4}<HS*3>7,")),
            "paste restores the target inline meter");
    }
    const auto replace = pasteNoteFragment(target, caret, caret, copied,
        {{caret, caret + 1}, {caret + 2, caret + 3}}, targetModel.noteSourceRanges());
    const auto parsedReplace = SimaiParser::validateSyntax(replace.text);
    expect(parsedReplace.ok && parsedReplace.noteMarkers.size() == 4,
        "noncontiguous collection replacement inserts the copied fragment once");
    const QString defaultHs = QStringLiteral("(120){4}2,\nE");
    TimelineQuickModel defaultHsModel;
    defaultHsModel.rebuildFromText(defaultHs, 0);
    const auto plain = copySelectedNotes(defaultHs, {{8,9}}, defaultHsModel.noteSourceRanges());
    expect(plain.contains(QStringLiteral("<HS*1>")) && !plain.trimmed().endsWith(QLatin1Char('E')),
        "plain clipboard fragment explicitly resets inherited HS and has no terminal marker");
    const QString grouped = QStringLiteral("(120){4}1/3,4h[4:1],|| words\n8,\nE");
    TimelineQuickModel groupedModel;
    groupedModel.rebuildFromText(grouped, 0);
    for (int at : {grouped.indexOf(QLatin1Char('/')), grouped.indexOf(QStringLiteral("h[")) + 2,
        grouped.indexOf(QStringLiteral("words")) + 2, grouped.indexOf(QStringLiteral(",4h"))}) {
        const auto insertion = pasteNoteFragment(grouped, at, at, plain, {}, groupedModel.noteSourceRanges());
        const auto parsedInsertion = SimaiParser::validateSyntax(insertion.text);
        expect(parsedInsertion.ok && parsedInsertion.noteMarkers.size() == 5,
            "pasting at separators, within tokens, after notes or in comments preserves original tokens and remains valid");
    }
    const QString inlineTerminal = QStringLiteral("(120){4}1,2,e");
    TimelineQuickModel inlineModel;
    inlineModel.rebuildFromText(inlineTerminal, 0);
    const auto afterFirst = pasteNoteFragment(inlineTerminal, 9, 9, plain, {}, inlineModel.noteSourceRanges());
    const auto afterFirstParsed = SimaiParser::parseForTimeline(afterFirst.text);
    expect(afterFirstParsed.ok && afterFirstParsed.noteMarkers.size() == 3
        && qAbs(afterFirstParsed.noteMarkers.last().second - 1.0) < 1e-7,
        "pasting at an existing comma inserts exactly the fragment duration");
    for (const QString& gap : {QStringLiteral("  "), QStringLiteral("\n  "), QStringLiteral(" || text, comma\n")}) {
        const QString targetWithGap = QStringLiteral("(120){4}1") + gap + QStringLiteral(",2,\nE");
        TimelineQuickModel gapModel;
        gapModel.rebuildFromText(targetWithGap, 0);
        const auto gapPaste = pasteNoteFragment(targetWithGap, 9, 9, plain, {}, gapModel.noteSourceRanges());
        const auto parsedGapPaste = SimaiParser::parseForTimeline(gapPaste.text);
        expect(parsedGapPaste.ok && parsedGapPaste.noteMarkers.size() == 3
            && qAbs(parsedGapPaste.noteMarkers.last().second - 1.0) < 1e-7,
            "pasting before a spaced or commented comma does not add an extra beat");
    }
    const QString timedGap = QStringLiteral("(120){4}1\n(240){8}<HS*2>,3,\nE");
    TimelineQuickModel timedGapModel;
    timedGapModel.rebuildFromText(timedGap, 0);
    const auto timedGapPaste = pasteNoteFragment(timedGap, 9, 9, plain, {}, timedGapModel.noteSourceRanges());
    const auto timedGapParsed = SimaiParser::parseForTimeline(timedGapPaste.text);
    expect(timedGapParsed.ok && timedGapParsed.noteMarkers.size() == 3
        && qAbs(timedGapParsed.noteMarkers.last().second - 0.625) < 1e-7
        && timedGapParsed.noteMarkers.last().hsMultiplier == 2.0,
        "a reused comma retains the target directives before it");
    const auto atEnd = pasteNoteFragment(inlineTerminal, inlineTerminal.size(), inlineTerminal.size(),
        plain, {}, inlineModel.noteSourceRanges());
    expect(SimaiParser::validateSyntax(atEnd.text).ok && atEnd.text.endsWith(QLatin1Char('e')),
        "pasting past the terminal inserts before it");
    const auto atPackedSibling = pasteNoteFragment(packed, 9, 9, plain, {}, packedModel.noteSourceRanges());
    const auto packedParsed = SimaiParser::parseForTimeline(atPackedSibling.text);
    expect(packedParsed.ok && packedParsed.noteMarkers.size() == 4
        && packedParsed.noteMarkers[1].second == packedParsed.noteMarkers[3].second,
        "pasting between packed notes preserves their simultaneous beat");
    const auto onlyDirective = pasteNoteFragment(inlineTerminal, 1, 3, plain, {}, inlineModel.noteSourceRanges());
    expect(onlyDirective.text.endsWith(inlineTerminal) && SimaiParser::validateSyntax(onlyDirective.text).ok,
        "a note fragment cannot replace part of a timing directive");
    const auto invalid = pasteNoteFragment(inlineTerminal, 9, 9, QStringLiteral("{0}2,"),
        {}, inlineModel.noteSourceRanges());
    expect(invalid.text == inlineTerminal, "malformed dedicated clipboard input leaves the chart untouched");
    return failures == 0 ? 0 : 1;
}
