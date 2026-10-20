#pragma once

#include "editor/SimaiTextEditPolicy.h"

#include <QObject>
#include <QString>

namespace miacode::ui {

// DSL smart input, completion catalog and bookmark comment commands.
class EditorController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool halfWidthInputEnabled READ halfWidthInputEnabled NOTIFY settingsChanged)
    Q_PROPERTY(bool overwriteMode READ overwriteMode NOTIFY settingsChanged)
    Q_PROPERTY(bool autoCompletionEnabled READ autoCompletionEnabled NOTIFY settingsChanged)
    Q_PROPERTY(bool imeInputDisabled READ imeInputDisabled NOTIFY settingsChanged)
    Q_PROPERTY(QString wholeBpm READ wholeBpm NOTIFY settingsChanged)
    Q_PROPERTY(bool completionActive READ completionActive NOTIFY completionChanged)
    Q_PROPERTY(QStringList completionCandidates READ completionCandidates NOTIFY completionChanged)
    Q_PROPERTY(int completionIndex READ completionIndex NOTIFY completionChanged)
    Q_PROPERTY(QString completionGroup READ completionGroup NOTIFY completionChanged)

public:
    explicit EditorController(QObject* parent = nullptr);

    bool halfWidthInputEnabled() const;
    bool overwriteMode() const;
    bool autoCompletionEnabled() const;
    bool imeInputDisabled() const;
    QString wholeBpm() const;
    bool completionActive() const;
    QStringList completionCandidates() const;
    int completionIndex() const;
    QString completionGroup() const;

    void setHalfWidthInputEnabled(bool enabled);
    void setOverwriteMode(bool enabled);
    void setAutoCompletionEnabled(bool enabled);
    void setCompletionConfig(const miacode::editor::SimaiCompletionConfig& config);
    void setImeInputDisabled(bool disabled);
    void setWholeBpm(const QString& bpm);

    miacode::editor::SimaiTextEditResult processKey(
        const QString& text, int anchor, int position, const QString& input, int key, int modifiers);
    miacode::editor::SimaiTextEditResult acceptCompletion(const QString& text, int anchor, int position);
    void setDifficulty(int difficultyId);
    void triggerCompletion(QChar glyph, const QString& text, int position, bool closingPresent);

    miacode::editor::SimaiTextEditResult createBookmark(const QString& lineText, int offset, const QString& title) const;
    miacode::editor::SimaiTextEditResult renameBookmark(const QString& lineText, int offset, const QString& title) const;
    miacode::editor::SimaiTextEditResult deleteBookmark(const QString& lineText, int offset) const;
    miacode::editor::SimaiTextEditResult touchPadAuthoring(const QString& text, int anchor,
                                                        int position, const QString& pad, QChar separator) const;
    void updateCompletion(const QString& text, int position);
    Q_INVOKABLE void moveCompletionSelection(int delta);
    Q_INVOKABLE void selectCompletionIndex(int index);
    Q_INVOKABLE void closeCompletion();

signals:
    void settingsChanged();
    void completionChanged();

private:
    miacode::editor::SimaiTextEditResult process(const miacode::editor::SimaiTextEditRequest& request);
    void setCompletion(const miacode::editor::SimaiCompletionSession& completion);
    void filterCompletion(const QString& text, int position);
    bool halfWidthInputEnabled_ = true;
    bool overwriteMode_ = false;
    bool autoCompletionEnabled_ = true;
    bool imeInputDisabled_ = true;
    QString wholeBpm_;
    miacode::editor::SimaiCompletionConfig completionConfig_ = miacode::editor::defaultCompletionConfig();
    miacode::editor::SimaiCompletionSession completion_;
    QStringList visibleCandidates_;
    int completionIndex_ = -1;
    int activeDifficultyId_ = -1;
};

} // namespace miacode::ui
