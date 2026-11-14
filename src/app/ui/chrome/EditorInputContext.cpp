#include "app/ui/chrome/EditorInputContext.h"

#include <QQuickItem>
#include <QGuiApplication>
#include <QInputMethod>
#include <QKeyEvent>
#include <QMetaMethod>
#include <QEvent>
#include <QTimer>

#include <algorithm>
#include <memory>

namespace miacode::ui {
namespace {

bool hasMarker(QObject* object, const char* marker)
{
    for (QObject* current = object; current; current = current->parent()) {
        if (current->property(marker).toBool()) return true;
    }
    for (auto* item = qobject_cast<QQuickItem*>(object); item; item = item->parentItem()) {
        for (QObject* current = item; current; current = current->parent()) {
            if (current->property(marker).toBool()) return true;
        }
    }
    return false;
}

bool isTextControl(QObject* object)
{
    return object && (object->inherits("QQuickTextInput") || object->inherits("QQuickTextEdit"));
}

bool isTextInput(QObject* object)
{
    return isTextControl(object) && !hasMarker(object, "chartEditorInput");
}

// Printable characters with or without Shift, plus Backspace. Ctrl/Alt/Meta keep a key a command.
bool isTypingKey(const QKeyEvent* event)
{
    if (event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) return false;
    if (event->key() == Qt::Key_Backspace) return true;
    const QString text = event->text();
    return !text.isEmpty() && std::all_of(text.cbegin(), text.cend(), [](QChar c) { return c.isPrint(); });
}

bool insidePopup(QQuickItem* item)
{
    for (; item; item = item->parentItem()) {
        if (item->inherits("QQuickPopupItem")) return true;
    }
    return false;
}

} // namespace

EditorInputContext::EditorInputContext(QObject* parent) : QObject(parent)
{
    connect(qGuiApp, &QGuiApplication::focusObjectChanged, this, &EditorInputContext::refresh);
    connect(qGuiApp, &QGuiApplication::focusWindowChanged, this, &EditorInputContext::refresh);
    qGuiApp->installEventFilter(this);
}

void EditorInputContext::setWindow(QQuickWindow* window)
{
    if (window_ == window) return;
    disconnect(windowFocusConnection_);
    window_ = window;
    if (window_) {
        windowFocusConnection_ = connect(window_, &QQuickWindow::activeFocusItemChanged,
                                         this, &EditorInputContext::refresh);
    }
    emit windowChanged();
    refresh();
}

void EditorInputContext::setTypingTarget(QQuickItem* target)
{
    if (typingTarget_ == target) return;
    typingTarget_ = target;
    emit typingTargetChanged();
}

void EditorInputContext::watch(QObject* object, const QList<QByteArray>& signalSignatures,
                               QVector<QMetaObject::Connection>& connections)
{
    for (const auto& connection : connections) disconnect(connection);
    connections.clear();
    if (!object) return;
    const auto slot = metaObject()->method(metaObject()->indexOfSlot("refresh()"));
    for (const auto& signature : signalSignatures) {
        const int index = object->metaObject()->indexOfSignal(signature.constData());
        if (index >= 0) connections.append(connect(object, object->metaObject()->method(index), this, slot));
    }
    // Quick must remove a dying item from its focus chain before we inspect it.
    connections.append(connect(object, &QObject::destroyed, this, &EditorInputContext::refresh,
                               Qt::QueuedConnection));
}

void EditorInputContext::refresh()
{
    auto* activeWindow = qobject_cast<QQuickWindow*>(QGuiApplication::focusWindow());
    if (!activeWindow) activeWindow = window_;
    QObject* focused = activeWindow ? activeWindow->activeFocusItem() : nullptr;
    if (focusedItem_ != focused) {
        focusedItem_ = focused;
        watch(focused, {"inputMethodComposingChanged()", "imeComposingChanged()"}, focusConnections_);
    }
    modalBlocked_ = QGuiApplication::modalWindow() != nullptr
        || hasMarker(focused, "blocksWorkbenchShortcuts")
        || (activeWindow && activeWindow != window_
            && activeWindow->objectName() != QStringLiteral("detachedPreviewWindow"));
    composing_ = focused && (focused->property("inputMethodComposing").toBool()
                              || focused->property("imeComposing").toBool());
    // Menus keep the editor/input that opened them as their edit command target.
    if (!hasMarker(focused, "preservesEditTarget")) {
        QObject* input = isTextInput(focused) ? focused : nullptr;
        if (textInput_ != input) {
            textInput_ = input;
            watch(input, {"textChanged()", "selectionChanged()", "selectionStartChanged()",
                          "selectionEndChanged()", "readOnlyChanged()", "canUndoChanged()",
                          "canRedoChanged()", "canPasteChanged()", "echoModeChanged()",
                          "enabledChanged()"}, inputConnections_);
        }
    }
    emit contextChanged();
}

bool EditorInputContext::workbenchCommandsAllowed() const
{
    return window_ && !modalBlocked_ && !composing_ && !textInput_;
}

bool EditorInputContext::nonTextCommandsAllowed() const
{
    // The chart editor is a text control too; controls that keep Space opt in with a marker.
    return workbenchCommandsAllowed() && !isTextControl(focusedItem_)
        && !hasMarker(focusedItem_, "reservesPlainSpace")
        && !hasMarker(focusedItem_, "preservesEditTarget");
}

bool EditorInputContext::typingTargetWritable() const
{
    // ScintillaQuick spells its read-only flag "readonly".
    return typingTarget_ && typingTarget_->isVisible() && typingTarget_->isEnabled()
        && !typingTarget_->property("readonly").toBool();
}

bool EditorInputContext::redirectTyping(QKeyEvent* event)
{
    QQuickItem* target = typingTarget_;
    QQuickItem* focused = window_->activeFocusItem();
    // Text inputs, menus, dialogs and IME composition keep their keys.
    if (!target || target->window() != window_ || !typingTargetWritable()
        || !workbenchCommandsAllowed() || insidePopup(focused) || !isTypingKey(event)
        || focused == target || target->isAncestorOf(focused)) {
        return false;
    }
    // Before focus hides it, a visible follow caret (e.g. after scrubbing the timeline)
    // decides where typing continues.
    if (target->metaObject()->indexOfMethod("continueAtFollowCaret()") >= 0)
        QMetaObject::invokeMethod(target, "continueAtFollowCaret", Qt::DirectConnection);
    target->forceActiveFocus(Qt::OtherFocusReason);
    if (!target->hasActiveFocus()) return false;
    // Delivered straight to the editor, so shortcuts are not matched a second time.
    const std::unique_ptr<QKeyEvent> typed(event->clone());
    QCoreApplication::sendEvent(target, typed.get());
    event->accept();
    return true;
}

bool EditorInputContext::eventFilter(QObject* object, QEvent* event)
{
    switch (event->type()) {
    case QEvent::InputMethod:
        QTimer::singleShot(0, this, &EditorInputContext::refresh);
        break;
    case QEvent::ShortcutOverride:
        // Like other text controls, the focused chart editor types before matching shortcuts.
        if (object == typingTarget_ && typingTargetWritable()
            && isTypingKey(static_cast<QKeyEvent*>(event))) {
            event->accept();
            return true;
        }
        break;
    case QEvent::KeyPress:
        // A key reaches the root item only after shortcuts and every focused item declined it.
        if (window_ && object == window_->contentItem())
            return redirectTyping(static_cast<QKeyEvent*>(event));
        break;
    default:
        break;
    }
    return false;
}

bool EditorInputContext::canEdit(const QString& operation) const
{
    if (!textInput_ || !textInput_->property("enabled").toBool()) return false;
    const bool writable = !textInput_->property("readOnly").toBool();
    const bool selected = textInput_->property("selectionStart") != textInput_->property("selectionEnd");
    const bool sensitive = textInput_->property("echoMode").isValid()
        && textInput_->property("echoMode").toInt() != 0;
    if (operation == "undo") return writable && textInput_->property("canUndo").toBool();
    if (operation == "redo") return writable && textInput_->property("canRedo").toBool();
    if (operation == "cut") return writable && selected && !sensitive;
    if (operation == "copy") return selected && !sensitive;
    if (operation == "paste") return writable && textInput_->property("canPaste").toBool();
    return operation == "selectAll" && !textInput_->property("text").toString().isEmpty();
}

bool EditorInputContext::performEdit(const QString& operation)
{
    return canEdit(operation)
        && QMetaObject::invokeMethod(textInput_, operation.toUtf8().constData(), Qt::DirectConnection);
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
