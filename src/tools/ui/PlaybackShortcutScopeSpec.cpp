#include "app/ui/chrome/EditorInputContext.h"
#include "app/ui/chrome/ShortcutModel.h"
#include "app/services/ShortcutRegistry.h"

#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlContext>
#include <QTest>
#include <QTemporaryDir>
#include <QTextStream>

// Plain Space is an ordinary registry binding (preview.play_pause_non_text), not a
// hard-coded gesture. The contract: it plays outside text/menus/dialogs, it can be
// rebound, and it yields to any other command that owns the same keys.
// Typing keys belong to the chart editor: it types them before shortcuts, and typing
// that no shortcut takes in a neutral area returns to it.
int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    QTemporaryDir storage;
    miacode::ui::ShortcutRegistry registry(storage.filePath("shortcuts.json"));
    miacode::ui::ShortcutModel shortcuts(registry);
    miacode::ui::EditorInputContext input;
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("testShortcuts", &shortcuts);
    engine.rootContext()->setContextProperty("testInput", &input);
    QQmlComponent component(&engine);
    component.setData(QByteArray(R"(
import QtQuick
import QtQuick.Controls
import "file:CHROME_DIR" as Chrome
Window {
    id: main; width: 320; height: 200; visible: true
    property int toggles: 0
    property int stops: 0
    QtObject {
        id: preview
        function stopOrPlay() { main.stops++ }
        function togglePlayback() { main.toggles++ }
        function adjustRate(direction) {}
    }
    Chrome.PreviewShortcuts {
        shortcuts: testShortcuts; previewSession: preview; inputContext: testInput
        commandsEnabled: testInput.workbenchCommandsAllowed
    }
    // Like ScintillaQuick, the chart editor is not a Qt text control and claims no keys itself.
    Item {
        objectName: "source"; property bool chartEditorInput: true; property bool reservesPlainSpace: true
        property bool readonly: false; property string typed: ""
        property int unfocusedContinues: 0
        function continueAtFollowCaret() { if (!activeFocus) unfocusedContinues++ }
        Keys.onPressed: event => {
            typed += event.key === Qt.Key_Backspace ? "<" : event.text
            event.accepted = true
        }
    }
    TextInput { objectName: "field"; text: "field" }
    Popup { objectName: "popup"; Item { objectName: "popupItem"; focus: true } }
    Item { objectName: "neutral" }
    Item { objectName: "menu"; property bool preservesEditTarget: true }
    Item { objectName: "modal"; property bool blocksWorkbenchShortcuts: true }
    Window {
        objectName: "detachedPreviewWindow"; width: 200; height: 200
        visible: true; transientParent: null
        Chrome.PreviewShortcuts {
            shortcuts: testShortcuts; previewSession: preview; inputContext: testInput
            commandsEnabled: testInput.workbenchCommandsAllowed
        }
        Item { objectName: "neutral" }
    }
}
)").replace("CHROME_DIR", MIACODE_SOURCE_ROOT "/src/app/ui/chrome"), QUrl());
    QScopedPointer<QObject> object(component.create());
    auto* window = qobject_cast<QQuickWindow*>(object.data());
    QTextStream out(stdout);
    bool passed = true;
    const auto check = [&](bool result, const char* label) {
        out << (result ? "PASS " : "FAIL ") << label << Qt::endl;
        passed = result && passed;
    };
    if (!window) { out << component.errorString(); return 1; }
    input.setWindow(window);
    check(shortcuts.sequence("preview.play_pause_non_text") == "Space",
          "outside-editor playback is a registered Space binding");

    for (auto* active : {window, window->findChild<QQuickWindow*>("detachedPreviewWindow")}) {
        active->requestActivate();
        QTest::qWait(80);
        const auto press = [&](Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
            QTest::keyClick(active, key, modifiers);
        };
        const auto toggled = [&] {
            const int count = window->property("toggles").toInt();
            window->setProperty("toggles", 0);
            return count;
        };
        const auto focus = [&](const char* name) {
            auto* item = window->findChild<QQuickItem*>(name);
            if (active != window && QByteArray(name) == "neutral")
                item = active->findChild<QQuickItem*>(name);
            item->forceActiveFocus();
            QCoreApplication::processEvents();
            return item;
        };

        auto* neutral = focus("neutral");
        press(Qt::Key_Space);
        check(toggled() == 1 && active->activeFocusItem() == neutral,
              "Space plays once outside text and leaves focus alone");

        if (active == window) {
            for (const char* name : {"source", "field", "menu", "modal"}) {
                focus(name);
                press(Qt::Key_Space);
                check(toggled() == 0, name);
            }
            shortcuts.setCaptureActive(true);
            focus("neutral");
            press(Qt::Key_Space);
            shortcuts.setCaptureActive(false);
            check(toggled() == 0, "shortcut capture blocks playback");
        }

        focus("neutral");
        check(shortcuts.setShortcutText("preview.play_pause_non_text", "Ctrl+Alt+C"),
              "accept a custom outside-editor key");
        press(Qt::Key_Space);
        press(Qt::Key_C, Qt::ControlModifier | Qt::AltModifier);
        check(toggled() == 1, "rebinding releases Space instead of keeping a fallback");

        shortcuts.resetShortcut("preview.play_pause_non_text");
        shortcuts.setShortcutText("preview.stop_or_play", "Space");
        press(Qt::Key_Space);
        check(toggled() == 0 && window->property("stops").toInt() == 1
                  && shortcuts.sequence("preview.play_pause_non_text").isEmpty(),
              "assigning Space to another command takes it from outside-editor playback");
        window->setProperty("stops", 0);
        shortcuts.resetShortcut("preview.stop_or_play");
        shortcuts.resetShortcut("preview.play_pause_non_text");

        // Configurations saved before bindings became exclusive may still share a key.
        const auto legacyBinding = [&](const QString& id, const QString& text) {
            text.isEmpty() ? registry.resetUserShortcut(id) : registry.setUserShortcutText(id, text);
            shortcuts.reload();
        };
        legacyBinding("preview.stop_or_play", "Space");
        press(Qt::Key_Space);
        check(toggled() == 0 && window->property("stops").toInt() == 1,
              "a legacy duplicate on Space leaves it to the other command");
        window->setProperty("stops", 0);
        legacyBinding("preview.stop_or_play", QString());

        legacyBinding("preview.play_pause_global", "Space");
        press(Qt::Key_Space);
        check(toggled() == 1, "both play/pause bindings on the same key run once");
        legacyBinding("preview.play_pause_global", QString());
    }

    auto* source = window->findChild<QQuickItem*>("source");
    auto* neutral = window->findChild<QQuickItem*>("neutral");
    input.setTypingTarget(source);
    source->setProperty("typed", QString());
    window->requestActivate();
    QTest::qWait(80);
    const auto typeFrom = [&](const char* name, Qt::Key key,
                              Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        auto* item = window->findChild<QQuickItem*>(name);
        if (!item) return QStringLiteral("<missing item>");
        item->forceActiveFocus();
        QCoreApplication::processEvents();
        QTest::keyClick(window, key, modifiers);
        const QString typed = source->property("typed").toString();
        source->setProperty("typed", QString());
        return typed;
    };

    shortcuts.setShortcutText("preview.stop_or_play", "Space");
    check(typeFrom("source", Qt::Key_Space) == " " && window->property("stops").toInt() == 0,
          "the chart editor types a bound Space instead of running the shortcut");
    check(typeFrom("neutral", Qt::Key_Space).isEmpty() && window->property("stops").toInt() == 1
              && window->activeFocusItem() == neutral,
          "outside the editor a bound Space stays a shortcut");
    window->setProperty("stops", 0);
    shortcuts.resetShortcut("preview.stop_or_play");

    check(typeFrom("neutral", Qt::Key_ParenLeft, Qt::ShiftModifier) == "("
              && window->activeFocusItem() == source,
          "an unbound character returns to the editor and is typed there");
    check(source->property("unfocusedContinues").toInt() == 1,
          "returning typing asks the editor to continue at its follow caret before focusing it");
    check(typeFrom("neutral", Qt::Key_Backspace) == "<", "Backspace returns to the editor");
    shortcuts.setShortcutText("preview.play_pause_non_text", "Ctrl+Alt+C");
    check(typeFrom("neutral", Qt::Key_Space) == " ", "an unbound Space returns to the editor");
    shortcuts.resetShortcut("preview.play_pause_non_text");

    bool kept = typeFrom("neutral", Qt::Key_A, Qt::ControlModifier).isEmpty()
        && window->activeFocusItem() == neutral;
    for (const Qt::Key key : {Qt::Key_Return, Qt::Key_Escape, Qt::Key_Delete, Qt::Key_Left, Qt::Key_F2})
        kept = typeFrom("neutral", key).isEmpty() && window->activeFocusItem() == neutral && kept;
    check(kept, "command and modified keys do not return to the editor");

    QObject* popup = window->findChild<QObject*>("popup");
    QMetaObject::invokeMethod(popup, "open");
    QCoreApplication::processEvents();
    bool stayed = true;
    for (const char* name : {"field", "modal", "popupItem"}) {
        stayed = typeFrom(name, Qt::Key_A).isEmpty()
            && window->activeFocusItem() == window->findChild<QQuickItem*>(name) && stayed;
    }
    QMetaObject::invokeMethod(popup, "close");
    source->setProperty("readonly", true);
    stayed = typeFrom("neutral", Qt::Key_A).isEmpty() && stayed;
    source->setProperty("readonly", false);
    check(stayed, "text inputs, dialogs, popups and a read-only editor keep typing away from the editor");
    return passed ? 0 : 1;
}
