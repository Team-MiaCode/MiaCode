#include "app/runtime/shell/PlaybackSpaceFilter.h"
#include "app/ui/chrome/ShortcutModel.h"

#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlContext>
#include <QTest>
#include <QTemporaryDir>
#include <QTextStream>
#include <memory>

class Harness : public QObject {
public:
    Harness(const miacode::ui::ShortcutRegistry& registry, QQuickWindow& window)
        : window(window), filter(registry, [this] { ++toggles; }, [this] { ++stops; }) {}
    bool eventFilter(QObject* target, QEvent* event) override
    {
        return target == &window && filter.handle(event, &window, true);
    }
    void space(bool repeat = false)
    {
        QKeyEvent press(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, " ", repeat);
        QCoreApplication::sendEvent(&window, &press);
        QKeyEvent release(QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier, " ");
        QCoreApplication::sendEvent(&window, &release);
    }
    QQuickWindow& window;
    miacode::runtime::PlaybackSpaceFilter filter;
    int toggles = 0;
    int stops = 0;
};

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    QTemporaryDir directory;
    miacode::ui::ShortcutRegistry registry(directory.filePath("shortcuts.json"));
    QQuickWindow window;
    window.setProperty("playbackCommandsEnabled", true);
    window.resize(400, 240);
    window.show();
    window.requestActivate();
    QCoreApplication::processEvents();
    QQuickItem neutral(window.contentItem());
    neutral.forceActiveFocus();
    Harness harness(registry, window);
    app.installEventFilter(&harness);
    QTextStream out(stdout);
    bool passed = true;
    const auto check = [&](bool result, const char* label) {
        out << (result ? "PASS " : "FAIL ") << label << Qt::endl;
        passed = result && passed;
    };
    check(!registry.editableShortcuts().isEmpty(), "production shortcut defaults loaded");
    harness.space();
    check(harness.toggles == 1 && harness.stops == 0, "default Space toggles playback");
    check(registry.setUserShortcutText("preview.stop_or_play", "Space"), "accept custom Stop/Play Space binding");
    harness.space();
    check(harness.stops == 1 && harness.toggles == 1, "custom Space executes Stop or Play, not Play/Pause");
    const int beforeRepeat = harness.stops;
    harness.space(true);
    check(harness.stops == beforeRepeat, "auto-repeat does not repeat playback commands");
    registry.resetUserShortcut("preview.stop_or_play");
    check(registry.setUserShortcutText("transform.rotate_cw_45", "Space"), "accept other command Space binding");
    QKeyEvent other(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, " ");
    check(!harness.filter.handle(&other, &window, true), "Space assigned to another command is not swallowed");
    registry.resetUserShortcut("transform.rotate_cw_45");
    registry.setUserShortcutText("transform.rotate_cw_45", "Space, A");
    QKeyEvent chord(QEvent::ShortcutOverride, Qt::Key_Space, Qt::NoModifier, " ");
    check(!harness.filter.handle(&chord, &window, true), "multi-key shortcut beginning with Space is not swallowed");
    registry.resetUserShortcut("transform.rotate_cw_45");
    registry.setUserShortcutText("preview.play_pause_global", "Space");
    const int beforeToggle = harness.toggles;
    harness.space();
    check(harness.toggles == beforeToggle + 1, "explicit Play/Pause Space fires once");
    registry.setUserShortcutText("preview.play_pause_global", "Ctrl+Shift+X");
    const int beforeDefault = harness.toggles;
    harness.space();
    check(harness.toggles == beforeDefault + 1, "reassigned binding immediately restores default Space");
    registry.setUserShortcutText("preview.stop_or_play", "Space");
    const int beforeHotChange = harness.stops;
    harness.space();
    registry.setUserShortcutText("preview.stop_or_play", "Ctrl+K");
    const int beforeFallback = harness.toggles;
    harness.space();
    check(harness.stops == beforeHotChange + 1 && harness.toggles == beforeFallback + 1,
          "Stop/Play Space binding updates without restarting the filter");
    window.setProperty("playbackCommandsEnabled", false);
    QKeyEvent disabled(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, " ");
    check(!harness.filter.handle(&disabled, &window, true), "disabled playback leaves Space alone");
    window.setProperty("playbackCommandsEnabled", true);
    QKeyEvent external(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, " ");
    check(!harness.filter.handle(&external, &window, false), "other windows retain Space");
    QKeyEvent modified(QEvent::KeyPress, Qt::Key_Space, Qt::ControlModifier, " ");
    check(!harness.filter.handle(&modified, &window, true), "modified Space is not a plain Space command");
    registry.setUserShortcutText("preview.stop_or_play", "Space");
    window.setProperty("sourceEditorFocused", true);
    QKeyEvent editor(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, " ");
    check(!harness.filter.handle(&editor, &window, true), "editor keeps ownership of Space");
    window.setProperty("sourceEditorFocused", false);

    QQmlEngine engine;
    miacode::ui::ShortcutModel shortcuts(registry);
    engine.rootContext()->setContextProperty("shortcutModel", &shortcuts);
    QQmlComponent bindings(&engine);
    bindings.setData(R"(
        import QtQuick
        import "." as Chrome
        Item {
            id: fixture
            property int shortcutStops: 0
            property int shortcutToggles: 0
            Chrome.ShortcutBindings {
                shortcuts: shortcutModel
                commands: QtObject { function shortcutCommandIds() { return [] } }
                previewSession: QtObject {
                    function stopOrPlay() { fixture.shortcutStops++ }
                    function togglePlayback() { fixture.shortcutToggles++ }
                    function adjustRate(delta) {}
                }
            }
        }
    )", QUrl::fromLocalFile(QStringLiteral(MIACODE_SOURCE_ROOT "/src/app/ui/chrome/SpaceFixture.qml")));
    std::unique_ptr<QObject> bindingObject(bindings.create());
    auto* bindingItem = qobject_cast<QQuickItem*>(bindingObject.get());
    if (!bindingItem) out << bindings.errorString() << Qt::endl;
    check(bindingItem != nullptr, "load production ShortcutBindings");
    if (bindingItem) bindingItem->setParentItem(window.contentItem());
    QCoreApplication::processEvents();
    neutral.forceActiveFocus();
    const int beforeJoint = harness.stops;
    QTest::keyClick(&window, Qt::Key_Space);
    QCoreApplication::processEvents();
    check(bindingObject && harness.stops - beforeJoint
          + bindingObject->property("shortcutStops").toInt() == 1
          && bindingObject->property("shortcutToggles").toInt() == 0,
          "real shortcut map and global filter execute custom Space exactly once");
    window.setProperty("sourceEditorFocused", true);
    const int beforeEditor = harness.stops;
    const int beforeEditorShortcut = bindingObject ? bindingObject->property("shortcutStops").toInt() : 0;
    QTest::keyClick(&window, Qt::Key_Space);
    QCoreApplication::processEvents();
    check(bindingObject && harness.stops == beforeEditor
          && bindingObject->property("shortcutStops").toInt() == beforeEditorShortcut + 1,
          "focused editor retains its configured preview Space shortcut");
    if (bindingObject) bindingObject->setProperty("shortcutStops", 0);
    window.setProperty("sourceEditorFocused", false);
    QQmlComponent component(&engine);
    component.setData("import QtQuick\nTextInput { width: 200; height: 40; property bool reservesPlainSpace: true }", QUrl());
    std::unique_ptr<QObject> field(component.create());
    auto* input = qobject_cast<QQuickItem*>(field.get());
    check(input != nullptr, "create input field");
    if (input) {
        input->setParentItem(window.contentItem());
        input->forceActiveFocus();
        const int before = harness.stops;
        shortcuts.resetShortcut("preview.stop_or_play");
        window.setProperty("sourceEditorFocused", true);
        QTest::keyClick(&window, Qt::Key_Space);
        QCoreApplication::processEvents();
        check(field->property("text").toString() == " " && harness.stops == before,
              "unassigned Space remains literal text in the focused editor");
        window.setProperty("sourceEditorFocused", false);
        shortcuts.setShortcutText("preview.stop_or_play", "Space");
        field->setProperty("text", "");
        QTest::keyClick(&window, Qt::Key_Space);
        QCoreApplication::processEvents();
        check(field->property("text").toString() == " " && harness.stops == before,
              "input field receives literal Space without playback");
        input->setProperty("reservesPlainSpace", false);
        QQuickItem dialog(window.contentItem());
        dialog.setProperty("reservesPlainSpace", true);
        input->setParentItem(&dialog);
        input->forceActiveFocus();
        QTest::keyClick(&window, Qt::Key_Space);
        QCoreApplication::processEvents();
        check(field->property("text").toString() == "  " && harness.stops == before,
              "dialog descendants retain Space without playback");
        check(bindingObject && bindingObject->property("shortcutStops").toInt() == 0,
              "production preview shortcut does not steal field or dialog Space");
        QQuickItem dialogFocus(&dialog);
        dialogFocus.forceActiveFocus();
        QTest::keyClick(&window, Qt::Key_Space);
        QCoreApplication::processEvents();
        check(harness.stops == before && bindingObject
              && bindingObject->property("shortcutStops").toInt() == 0,
              "dialog non-text focus also blocks the global Space shortcut");
        input->setParentItem(window.contentItem());
    }
    QQuickWindow detached;
    detached.resize(200, 200);
    QQuickItem previewFocus(detached.contentItem());
    detached.show();
    detached.requestActivate();
    previewFocus.forceActiveFocus();
    QTest::qWait(30);
    window.setProperty("sourceEditorFocused", true);
    registry.resetUserShortcut("preview.stop_or_play");
    const int beforeDetached = harness.toggles;
    QKeyEvent detachedSpace(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, " ");
    check(harness.filter.handle(&detachedSpace, &window, true)
          && harness.toggles == beforeDetached + 1 && detached.activeFocusItem() == &previewFocus,
          "Space uses the active preview focus instead of the inactive editor");
    previewFocus.setProperty("reservesPlainSpace", true);
    QKeyEvent reservedSpace(QEvent::ShortcutOverride, Qt::Key_Space, Qt::NoModifier, " ");
    check(harness.filter.handle(&reservedSpace, &window, true) && reservedSpace.isAccepted(),
          "active preview controls can reserve Space");
    app.removeEventFilter(&harness);
    return passed ? 0 : 1;
}
