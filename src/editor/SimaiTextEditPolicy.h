#pragma once

#include "editor/SimaiCompletionCatalog.h"

#include <QChar>
#include <QString>
#include <QStringList>
#include <Qt>

namespace miacode::editor {

// DSL editing plans use UTF-16 positions at the document service boundary.
struct SimaiTextEditRequest {
    QString text;
    int anchor = 0;
    int position = 0;
    QString input;
    int key = 0;
    Qt::KeyboardModifiers modifiers = Qt::NoModifier;
    bool halfWidthInputEnabled = true;
    bool overwriteMode = false;
    bool autoCompletionEnabled = true;
    QString wholeBpm;
    SimaiCompletionConfig completionConfig = defaultCompletionConfig();
    // An active completion treats typed characters as its filter prefix.
    bool completionActive = false;
};

struct SimaiTextEditTransaction {
    int anchor = 0;
    int position = 0;
    bool hasEdit = false;
    // Scintilla applies this span as one native undo action.
    int replacementStart = 0;
    int replacementEnd = 0;
    QString replacementText;
    int touchTokenStart = -1;
};

struct SimaiCompletionSession {
    bool active = false;
    QChar opening;
    bool closingPresent = false;
    int startPosition = -1;
    QStringList candidates;
};

struct SimaiTextEditResult {
    bool consumed = false;
    QString error;
    // Consumes command text that the native key path could otherwise insert.
    bool suppressFallbackInsert = false;
    SimaiTextEditTransaction transaction;
    SimaiCompletionSession completion;
};

QString normalizeSimaiInput(const QString& input);

SimaiTextEditResult applySimaiTextEditPolicy(const SimaiTextEditRequest& request);

} // namespace miacode::editor
