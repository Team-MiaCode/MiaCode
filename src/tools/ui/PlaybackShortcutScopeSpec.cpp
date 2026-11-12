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
    TextInput { objectName: "source"; text: "source"; property bool chartEditorInput: true }
    TextInput { objectName: "field"; text: "field" }
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
        check(toggled() == 0 && window->property("stops").toInt() == 1,
              "another command on Space takes precedence");
        window->setProperty("stops", 0);
        shortcuts.resetShortcut("preview.stop_or_play");

        shortcuts.setShortcutText("preview.play_pause_global", "Space");
        press(Qt::Key_Space);
        check(toggled() == 1, "both play/pause bindings on the same key run once");
        shortcuts.resetShortcut("preview.play_pause_global");
    }
    return passed ? 0 : 1;
}
