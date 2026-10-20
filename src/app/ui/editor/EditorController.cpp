#include "app/ui/editor/EditorController.h"

#include "editor/SimaiCompletionCatalog.h"
#include "editor/BookmarkCommentSyntax.h"
#include "editor/TouchPadAuthoringEdit.h"

#include <QtGlobal>

namespace miacode::ui {
namespace {

miacode::editor::SimaiTextEditResult untouched(const QString& text, int anchor, int position)
{
    miacode::editor::SimaiTextEditResult result;
    result.transaction.anchor = qBound(0, anchor, text.size());
    result.transaction.position = qBound(0, position, text.size());
    return result;
}

bool commandModifier(Qt::KeyboardModifiers modifiers)
{
    return modifiers & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
}

} // namespace

EditorController::EditorController(QObject* parent) : QObject(parent) {}
bool EditorController::halfWidthInputEnabled() const { return halfWidthInputEnabled_; }
bool EditorController::overwriteMode() const { return overwriteMode_; }
bool EditorController::autoCompletionEnabled() const { return autoCompletionEnabled_; }
bool EditorController::imeInputDisabled() const { return imeInputDisabled_; }
QString EditorController::wholeBpm() const { return wholeBpm_; }
bool EditorController::completionActive() const { return completion_.active; }
QStringList EditorController::completionCandidates() const { return visibleCandidates_; }
int EditorController::completionIndex() const { return completionIndex_; }

void EditorController::setHalfWidthInputEnabled(bool enabled)
{
    if (halfWidthInputEnabled_ == enabled) return;
    halfWidthInputEnabled_ = enabled;
    emit settingsChanged();
}
void EditorController::setOverwriteMode(bool enabled)
{
    if (overwriteMode_ == enabled) return;
    overwriteMode_ = enabled;
    closeCompletion();
    emit settingsChanged();
}
void EditorController::setAutoCompletionEnabled(bool enabled)
{
    if (autoCompletionEnabled_ == enabled) return;
    autoCompletionEnabled_ = enabled;
    if (!enabled) closeCompletion();
    emit settingsChanged();
}
void EditorController::setImeInputDisabled(bool disabled)
{
    if (imeInputDisabled_ == disabled) return;
    imeInputDisabled_ = disabled;
    emit settingsChanged();
}
QString EditorController::completionGroup() const
{
    if (!completion_.active) return {};
    if (completion_.opening == QLatin1Char('[')) return QStringLiteral("duration");
    if (completion_.opening == QLatin1Char('{')) return QStringLiteral("division");
    return {};
}

void EditorController::setCompletionConfig(const miacode::editor::SimaiCompletionConfig& config)
{
    if (completionConfig_ == config) return;
    completionConfig_ = config;
    closeCompletion();
}

void EditorController::setWholeBpm(const QString& bpm)
{
    if (wholeBpm_ == bpm) return;
    wholeBpm_ = bpm;
    emit settingsChanged();
}

miacode::editor::SimaiTextEditResult EditorController::processKey(
    const QString& text, int anchor, int position, const QString& input, int key, int modifiers)
{
    const auto keyboardModifiers = static_cast<Qt::KeyboardModifiers>(modifiers);
    if (completion_.active) {
        if (key == Qt::Key_Up) {
            moveCompletionSelection(-1);
            auto result = untouched(text, anchor, position);
            result.consumed = true;
            return result;
        }
        if (key == Qt::Key_Down) {
            moveCompletionSelection(1);
            auto result = untouched(text, anchor, position);
            result.consumed = true;
            return result;
        }
        if (key == Qt::Key_Escape) {
            closeCompletion();
            auto result = untouched(text, anchor, position);
            result.consumed = true;
            return result;
        }
        if ((key == Qt::Key_Return || key == Qt::Key_Enter || key == Qt::Key_Tab)
            && !commandModifier(keyboardModifiers))
            return acceptCompletion(text, anchor, position);
    }
    miacode::editor::SimaiTextEditRequest request;
    request.text = text;
    request.anchor = anchor;
    request.position = position;
    request.input = input;
    request.key = key;
    request.modifiers = keyboardModifiers;
    return process(request);
}

miacode::editor::SimaiTextEditResult EditorController::process(const miacode::editor::SimaiTextEditRequest& input)
{
    auto request = input;
    request.halfWidthInputEnabled = halfWidthInputEnabled_ && input.halfWidthInputEnabled;
    request.overwriteMode = overwriteMode_;
    request.autoCompletionEnabled = autoCompletionEnabled_ && request.autoCompletionEnabled;
    request.wholeBpm = wholeBpm_;
    request.completionConfig = completionConfig_;
    request.completionActive = completion_.active;
    auto result = miacode::editor::applySimaiTextEditPolicy(request);
    if (result.completion.active) setCompletion(result.completion);
    return result;
}

miacode::editor::SimaiTextEditResult EditorController::acceptCompletion(const QString& text, int anchor, int position)
{
    auto result = untouched(text, anchor, position);
    updateCompletion(text, position);
    if (!completion_.active || completionIndex_ < 0 || completionIndex_ >= visibleCandidates_.size()) return result;
    const int start = qBound(0, completion_.startPosition, text.size());
    int end = qBound(start, position, text.size());
    const QString candidate = visibleCandidates_.at(completionIndex_);
    // Catalog candidates include their closing glyph. When policy inserted a
    // matching pair, consume that existing right glyph in the same QML edit.
    const QChar closing = miacode::editor::closingBracketFor(completion_.opening);
    if (completion_.closingPresent && !closing.isNull() && end < text.size()
        && text.at(end) == closing) {
        ++end;
    }
    result.transaction.replacementStart = start;
    result.transaction.replacementEnd = end;
    result.transaction.replacementText = candidate;
    result.transaction.anchor = result.transaction.position = start + candidate.size();
    result.transaction.hasEdit = true;
    result.consumed = true;
    closeCompletion();
    return result;
}

void EditorController::triggerCompletion(QChar glyph, const QString& text, int position, bool closingPresent)
{
    if (!autoCompletionEnabled_ || overwriteMode_ || completion_.active) return;
    miacode::editor::SimaiCompletionSession completion;
    if (miacode::editor::isBracketOpening(glyph)) {
        completion.opening = glyph;
        completion.candidates = miacode::editor::candidatesForOpening(glyph, wholeBpm_, text, completionConfig_);
    } else if (glyph == QLatin1Char('h')) {
        if (position < text.size() && text.at(position) == QLatin1Char('[')) return;
        completion.opening = QLatin1Char('[');
        completion.candidates = miacode::editor::holdDurationCandidates(completionConfig_);
    } else return;
    completion.startPosition = position;
    completion.closingPresent = closingPresent;
    completion.active = !completion.candidates.isEmpty();
    setCompletion(completion);
}

void EditorController::setDifficulty(int difficultyId)
{
    if (activeDifficultyId_ != difficultyId) closeCompletion();
    activeDifficultyId_ = difficultyId;
}

void EditorController::setCompletion(const miacode::editor::SimaiCompletionSession& completion)
{
    completion_ = completion;
    visibleCandidates_ = completion.candidates;
    completionIndex_ = visibleCandidates_.isEmpty() ? -1 : 0;
    emit completionChanged();
}

void EditorController::filterCompletion(const QString& text, int position)
{
    const int start = completion_.startPosition;
    if (start < 0 || position < start || position > text.size()) { closeCompletion(); return; }
    const QString prefix = text.mid(start, position - start);
    const QChar closing = miacode::editor::closingBracketFor(completion_.opening);
    if (prefix.contains(QLatin1Char('\n')) || (!closing.isNull() && prefix.contains(closing))) { closeCompletion(); return; }
    QStringList candidates;
    const QString selected = completionIndex_ >= 0 && completionIndex_ < visibleCandidates_.size()
        ? visibleCandidates_.at(completionIndex_) : QString();
    for (const QString& candidate : completion_.candidates) {
        if (candidate.startsWith(prefix)) candidates.append(candidate);
    }
    if (candidates.isEmpty()) { closeCompletion(); return; }
    visibleCandidates_ = candidates;
    const int selectedIndex = candidates.indexOf(selected);
    completionIndex_ = selectedIndex >= 0 ? selectedIndex : 0;
    emit completionChanged();
}

void EditorController::updateCompletion(const QString& text, int position)
{
    if (completion_.active) filterCompletion(text, position);
}

void EditorController::moveCompletionSelection(int delta)
{
    if (!completion_.active || visibleCandidates_.isEmpty()) return;
    const int next = qBound(0, completionIndex_ + delta, visibleCandidates_.size() - 1);
    if (next == completionIndex_) return;
    completionIndex_ = next;
    emit completionChanged();
}

void EditorController::selectCompletionIndex(int index)
{
    if (!completion_.active || index < 0 || index >= visibleCandidates_.size()
        || completionIndex_ == index) {
        return;
    }
    completionIndex_ = index;
    emit completionChanged();
}

void EditorController::closeCompletion()
{
    if (!completion_.active && visibleCandidates_.isEmpty() && completionIndex_ == -1) return;
    completion_ = {};
    visibleCandidates_.clear();
    completionIndex_ = -1;
    emit completionChanged();
}

namespace {
miacode::editor::SimaiTextEditResult bookmarkEdit(const QString& lineText, int offset,
                                                const QString& replacement, bool caretAtEnd)
{
    miacode::editor::SimaiTextEditResult result;
    if (lineText == replacement) return result;
    result.consumed = true;
    auto& tx = result.transaction;
    tx.hasEdit = true;
    tx.replacementStart = offset;
    tx.replacementEnd = offset + lineText.size();
    tx.replacementText = replacement;
    tx.anchor = tx.position = offset + (caretAtEnd ? replacement.size() : 0);
    return result;
}
}
miacode::editor::SimaiTextEditResult EditorController::createBookmark(const QString& lineText, int offset, const QString& title) const
{
    return bookmarkEdit(lineText, offset, miacode::editor::appendBookmarkComment(lineText, title), true);
}
miacode::editor::SimaiTextEditResult EditorController::renameBookmark(const QString& lineText, int offset, const QString& title) const
{
    return bookmarkEdit(lineText, offset, miacode::editor::renameBookmarkComment(lineText, title), true);
}
miacode::editor::SimaiTextEditResult EditorController::deleteBookmark(const QString& lineText, int offset) const
{
    return bookmarkEdit(lineText, offset, miacode::editor::removeBookmarkComment(lineText), false);
}
miacode::editor::SimaiTextEditResult EditorController::touchPadAuthoring(
    const QString& text, int anchor, int position, const QString& pad, QChar separator) const
{
    const auto plan = miacode::editor::planTouchPadAuthoringEdit(text, position, pad, separator);
    auto result = untouched(text, anchor, position);
    if (!plan.valid) return result;
    const int start = qBound(0, plan.insertionPosition, text.size());
    const int end = qBound(start, start + plan.removalLength, text.size());
    result.consumed = true;
    auto& tx = result.transaction;
    tx.hasEdit = true;
    tx.replacementStart = start;
    tx.replacementEnd = end;
    tx.replacementText = plan.insertionText;
    tx.anchor = tx.position = start + plan.insertionText.size();
    tx.touchTokenStart = plan.tokenStart;
    return result;
}

} // namespace miacode::ui
