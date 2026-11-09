#pragma once

#include <scintillaquick/scintillaquick_item.h>
#include <QHash>
#include <QVector>
#include <optional>

namespace miacode::ui {

// Scintilla owns document storage and undo. This adapter retains one document
// reference and viewport per difficulty, and projects UTF-16 service offsets.
class ScintillaDocumentAdapter
{
public:
    explicit ScintillaDocumentAdapter(ScintillaQuick_item& editor);
    ~ScintillaDocumentAdapter();
    void activate(const QString& scope, const QString& text);
    void clear();
    void drop(const QString& scope);
    void refresh();
    void recordObjectSelectionForUndo();
    bool undo();
    void applyChange(int position, int removedUtf16, const QByteArray& inserted);
    int lineCount() const { return editor_.send(SCI_GETLINECOUNT); }
    int lineStart(int line) const { return editor_.send(SCI_INDEXPOSITIONFROMLINE, line, SC_LINECHARACTERINDEX_UTF16); }
    int lineAt(int position) const;
    void captureViewport();
    bool restoreViewport();
    int bytePosition(int utf16) const;
    int utf16Position(int byte) const;
    const QString& text() const { return text_; }
    const QString& scope() const { return scope_; }

private:
    struct Selection {
        int anchor;
        int caret;
    };
    struct UndoSelection {
        QVector<Selection> ranges;
        int mainSelection = 0;
        bool objectSelection = false;
    };
    struct Document {
        sptr_t pointer = 0;
        int anchor = 0;
        int caret = 0;
        int topPosition = 0;
        int xOffset = 0;
        QHash<int, UndoSelection> undoSelections;
    };
    void addUndoSelection(UndoSelection selection);
    void saveViewport();
    ScintillaQuick_item& editor_;
    QHash<QString, Document> documents_;
    QString scope_;
    QString text_;
    bool viewportPending_ = false;
    QMetaObject::Connection undoSelectionConnection_;
    std::optional<UndoSelection> selectionToRestore_;
    // Positive container tokens belong to touch-authoring preview anchors.
    int nextSelectionToken_ = -1;
};
}
