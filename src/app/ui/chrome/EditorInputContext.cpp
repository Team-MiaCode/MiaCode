#include "app/ui/chrome/EditorInputContext.h"

#include <QQuickItem>
#include <QGuiApplication>
#include <QInputMethod>

namespace miacode::ui {

void EditorInputContext::setWindow(QQuickWindow* window)
{
    if (window_ == window) return;
    window_ = window;
    emit windowChanged();
}

void EditorInputContext::commitPendingEdits(QQuickItem* editorPane)
{
    if (!window_ || !editorPane) return;
    QPointer<QQuickItem> input = window_->activeFocusItem();
    if (!input || (input != editorPane && !editorPane->isAncestorOf(input))) return;
    QGuiApplication::inputMethod()->commit();
    // Submit the focused field without ending its keyboard/selection session.
    for (QQuickItem* item = input; item; item = item->parentItem()) {
        if (item->metaObject()->indexOfMethod("commitPendingEdit()") >= 0) {
            QMetaObject::invokeMethod(item, "commitPendingEdit", Qt::DirectConnection);
            return;
        }
        if (item == editorPane) break;
    }
}

} // namespace miacode::ui
