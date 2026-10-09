#pragma once
#include <QObject>
#include <QQuickTextDocument>
#include <QTextDocument>
#include "app/ui/chrome/ShortcutCommands.h"

namespace miacode::android {
class AndroidEditorTools final : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    Q_INVOKABLE QStringList shortcutCommandIds() const { return ui::shortcutCommandIds(); }
    Q_INVOKABLE void clearHistory(QObject* wrapper)
    {
        auto* quickDocument = qobject_cast<QQuickTextDocument*>(wrapper);
        if (quickDocument && quickDocument->textDocument())
            quickDocument->textDocument()->clearUndoRedoStacks();
    }
};
}
