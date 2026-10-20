#include "core/chart/selection/ChartSelectionBeatSummary.h"

#include "core/chart/parser/SimaiCommentScan.h"

#include <QHash>
#include <algorithm>

namespace miacode::chart_selection {

namespace {

struct DenominatorCount {
    int denominator = 4;
    int count = 0;
};

bool isContentCharacter(const QString& text, const QByteArray& content, int index)
{
    return index >= 0 && index < text.size() && content.at(index) != 0;
}

bool isBoundaryTokenCharacter(const QString& text, const QByteArray& content, int index)
{
    if (!isContentCharacter(text, content, index)) {
        return false;
    }
    const QChar ch = text.at(index);
    return !ch.isSpace() && ch != QLatin1Char(',') && ch != QLatin1Char('{')
        && ch != QLatin1Char('}');
}

bool edgeCutsToken(const QString& text, const QByteArray& content, int edge)
{
    return edge > 0 && edge < text.size()
        && isBoundaryTokenCharacter(text, content, edge - 1)
        && isBoundaryTokenCharacter(text, content, edge);
}

}  // namespace

void ChartSelectionBeatIndex::setText(const QString& text)
{
    if (text_ == text) return;
    text_ = text;
    content_ = QByteArray(text.size(), '\0');
    commas_.clear();
    for (const auto& span : miacode::simai::chartContentSpans(text, 0, text.size()))
        for (int index = span.start; index < span.end; ++index) content_[index] = 1;
    int denominator = 4;
    for (int index = 0; index < text.size();) {
        if (!isContentCharacter(text, content_, index)) { ++index; continue; }
        if (text[index] == QLatin1Char('{')) {
            int cursor = index + 1, value = 0;
            bool hasDigit = false;
            while (cursor < text.size() && isContentCharacter(text, content_, cursor) && text[cursor].isDigit()) {
                hasDigit = true;
                value = value * 10 + text[cursor].digitValue();
                ++cursor;
            }
            if (hasDigit && cursor < text.size() && isContentCharacter(text, content_, cursor)
                && text[cursor] == QLatin1Char('}') && value > 0) {
                denominator = value;
                index = cursor + 1;
                continue;
            }
        }
        if (text[index] == QLatin1Char(',')) commas_.append({index, denominator});
        ++index;
    }
}
ChartSelectionBeatSummary ChartSelectionBeatIndex::summarize(int selectionStart, int selectionEnd) const
{
    ChartSelectionBeatSummary result;
    const int begin = qBound(0, qMin(selectionStart, selectionEnd), int(text_.size()));
    const int end = qBound(begin, qMax(selectionStart, selectionEnd), int(text_.size()));
    if (begin == end) return result;
    result.exact = !edgeCutsToken(text_, content_, begin) && !edgeCutsToken(text_, content_, end);
    QVector<DenominatorCount> counts;
    QHash<int, int> countIndex;
    auto comma = std::lower_bound(commas_.cbegin(), commas_.cend(), begin,
        [](const Comma& comma, int position) { return comma.position < position; });
    for (; comma != commas_.cend() && comma->position < end; ++comma) {
        ++result.totalCommaCount;
        const int existing = countIndex.value(comma->denominator, -1);
        if (existing >= 0) ++counts[existing].count;
        else { countIndex.insert(comma->denominator, counts.size()); counts.append({comma->denominator, 1}); }
    }
    for (const auto& entry : counts) result.parts.append({entry.count, entry.denominator});
    return result;
}
ChartSelectionBeatSummary summarizeChartSelectionBeats(const QString& text, int begin, int end)
{
    ChartSelectionBeatIndex index;
    index.setText(text);
    return index.summarize(begin, end);
}
}  // namespace miacode::chart_selection
