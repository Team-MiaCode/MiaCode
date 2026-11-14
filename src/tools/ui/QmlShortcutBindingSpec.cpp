#include "app/ui/document/ChartTransformCommands.h"
#include "app/ui/chrome/ShortcutCommands.h"
#include "app/ui/chrome/ShortcutModel.h"
#include "app/ui/chrome/EditorInputContext.h"
#include "app/services/ShortcutRegistry.h"
#include "app/runtime/playback/PreviewStopOrPlayPolicy.h"

#include <QDirIterator>
#include <QFile>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlExpression>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QTest>
#include <QKeySequence>
#include <QMetaEnum>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>
#include <QTemporaryDir>
#include <QJSValue>

#ifndef MIACODE_SOURCE_ROOT
#error "MIACODE_SOURCE_ROOT must be defined"
#endif

namespace {

bool expect(bool condition, const QString& message, QTextStream& out, int* failed)
{
    out << (condition ? "[PASS] " : "[FAIL] ") << message << '\n';
    out.flush();
    if (!condition) ++*failed;
    return condition;
}

void checkPendingInputCommit(QTextStream& out, int* failed)
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(R"(
import QtQuick
Item {
    width: 320; height: 200
    TextInput {
        objectName: "field"
        text: "pending"
        property string committedText: ""
        function commitPendingEdit() { committedText = text }
    }
}
)", QUrl());
    QScopedPointer<QObject> object(component.create());
    auto* pane = qobject_cast<QQuickItem*>(object.data());
    if (!expect(pane != nullptr, component.errorString(), out, failed)) return;
    QQuickWindow window;
    pane->setParentItem(window.contentItem());
    window.show();
    auto* field = pane->findChild<QQuickItem*>("field");
    field->forceActiveFocus();
    miacode::ui::EditorInputContext input;
    input.setWindow(&window);
    input.commitPendingEdits(pane);
    expect(field->property("committedText").toString() == "pending"
               && window.activeFocusItem() == field,
           QStringLiteral("committing pending input preserves its keyboard focus"), out, failed);
    pane->setParentItem(nullptr);
}

void checkInputOwnership(QTextStream& out, int* failed)
{
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
    width: 320; height: 200; visible: true
    property int executions: 0
    QtObject {
        id: preview
        function stopOrPlay() { executions++ }
        function togglePlayback() { executions++ }
        function adjustRate(direction) { executions++ }
    }
    QtObject { id: commands; function shortcutCommandIds() { return ["transform.mirror_lr"] } }
    TextInput { objectName: "field"; text: "field" }
    Item { objectName: "neutral" }
    Chrome.ShortcutBindings {
        shortcuts: testShortcuts; commands: commands; previewSession: preview
        commandsEnabled: testInput.workbenchCommandsAllowed
        onChartTransformRequested: executions++
    }
}
)").replace("CHROME_DIR", MIACODE_SOURCE_ROOT "/src/app/ui/chrome"), QUrl());
    QScopedPointer<QObject> object(component.create());
    auto* window = qobject_cast<QQuickWindow*>(object.data());
    if (!expect(window != nullptr, component.errorString(), out, failed)) return;
    input.setWindow(window);
    window->requestActivate();
    QTest::qWait(100);
    auto* field = window->findChild<QQuickItem*>("field");
    field->forceActiveFocus();
    QTest::keyClick(window, Qt::Key_J, Qt::ControlModifier);
    QTest::keyClick(window, Qt::Key_X, Qt::ControlModifier | Qt::ShiftModifier);
    expect(window->property("executions").toInt() == 0 && !input.workbenchCommandsAllowed(),
           QStringLiteral("text input owns keys instead of background chart commands"), out, failed);
    expect(input.textInput() == field && input.performEdit("selectAll")
               && field->property("selectedText").toString() == "field"
               && window->activeFocusItem() == field,
           QStringLiteral("editing targets the focused input without moving its focus"), out, failed);
    window->findChild<QQuickItem*>("neutral")->forceActiveFocus();
    QTest::keyClick(window, Qt::Key_J, Qt::ControlModifier);
    expect(window->property("executions").toInt() > 0,
           QStringLiteral("neutral workbench controls retain chart shortcuts"), out, failed);
    window->setProperty("executions", 0);
    shortcuts.setCaptureActive(true);
    QTest::keyClick(window, Qt::Key_J, Qt::ControlModifier);
    QTest::keyClick(window, Qt::Key_C, Qt::ControlModifier | Qt::ShiftModifier);
    expect(window->property("executions").toInt() == 0,
           QStringLiteral("shortcut capture excludes business commands"), out, failed);
}

void checkPreviewWindowOwnership(QTextStream& out, int* failed)
{
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
    property int executions: 0
    property bool allowed: true
    QtObject {
        id: preview
        function stopOrPlay() { executions++ }
        function togglePlayback() { executions++ }
        function adjustRate(direction) { executions++ }
    }
    Chrome.PreviewShortcuts {
        shortcuts: testShortcuts; previewSession: preview
        commandsEnabled: main.allowed && testInput && testInput.workbenchCommandsAllowed
    }
    TextInput { objectName: "field"; text: "input" }
    Item { objectName: "neutral" }
    Window {
        objectName: "detachedPreviewWindow"; width: 200; height: 200; visible: true; transientParent: null
        Chrome.PreviewShortcuts {
            shortcuts: testShortcuts; previewSession: preview
            commandsEnabled: main.allowed && testInput && testInput.workbenchCommandsAllowed
        }
        TextInput { objectName: "field"; text: "input" }
        Item { objectName: "neutral" }
    }
}
)").replace("CHROME_DIR", MIACODE_SOURCE_ROOT "/src/app/ui/chrome"), QUrl());
    QScopedPointer<QObject> object(component.create());
    auto* window = qobject_cast<QQuickWindow*>(object.data());
    if (!expect(window != nullptr, component.errorString(), out, failed)) return;
    input.setWindow(window);
    auto* detached = window->findChild<QQuickWindow*>("detachedPreviewWindow");
    for (auto* active : {window, detached}) {
        active->requestActivate();
        auto* neutral = active->findChild<QQuickItem*>("neutral");
        neutral->forceActiveFocus();
        QTest::qWait(100);
        const int before = window->property("executions").toInt();
        QTest::keyClick(active, Qt::Key_C, Qt::ControlModifier | Qt::ShiftModifier);
        QTest::keyClick(active, Qt::Key_X, Qt::ControlModifier | Qt::ShiftModifier);
        QTest::keyClick(active, Qt::Key_O, Qt::ControlModifier);
        QTest::keyClick(active, Qt::Key_P, Qt::ControlModifier);
        if (window->property("executions").toInt() != before + 4)
            out << "  preview executions=" << window->property("executions").toInt()
                << " before=" << before << " active=" << (QGuiApplication::focusWindow() == active) << '\n';
        expect(window->property("executions").toInt() == before + 4 && active->activeFocusItem() == neutral,
               QStringLiteral("each preview key executes once in the active window"), out, failed);
        auto* field = active->findChild<QQuickItem*>("field");
        field->forceActiveFocus();
        const int beforeInput = window->property("executions").toInt();
        QTest::keyClick(active, Qt::Key_P, Qt::ControlModifier);
        expect(window->property("executions").toInt() == beforeInput && active->activeFocusItem() == field,
               QStringLiteral("each preview window protects its text input and focus"), out, failed);
        neutral->forceActiveFocus();
    }
    shortcuts.setShortcutText("preview.speed_up", "Ctrl+Alt+9");
    window->setProperty("executions", 0);
    QTest::keyClick(detached, Qt::Key_P, Qt::ControlModifier);
    QTest::keyClick(detached, Qt::Key_9, Qt::ControlModifier | Qt::AltModifier);
    expect(window->property("executions").toInt() == 1,
           QStringLiteral("detached preview uses live custom bindings"), out, failed);
    window->setProperty("allowed", false);
    QTest::keyClick(detached, Qt::Key_9, Qt::ControlModifier | Qt::AltModifier);
    shortcuts.setCaptureActive(true);
    window->setProperty("allowed", true);
    QTest::keyClick(detached, Qt::Key_C, Qt::ControlModifier | Qt::ShiftModifier);
    expect(window->property("executions").toInt() == 1,
           QStringLiteral("preview windows honor command and capture guards"), out, failed);
}

struct MenuStandardKey {
    QString file;
    QString keyName;
    QKeySequence sequence;
};

// Every `shortcut: StandardKey.X` / `sequence: StandardKey.X` a v2 QML source
// declares. An Action or Shortcut spelled that way registers a real
// window-context binding, not just a menu label.
QList<MenuStandardKey> menuStandardKeys()
{
    static const QRegularExpression pattern(
        QStringLiteral("(?:shortcut|sequence)\\s*:\\s*StandardKey\\.(\\w+)"));
    const QMetaEnum meta = QMetaEnum::fromType<QKeySequence::StandardKey>();
    QList<MenuStandardKey> keys;
    QDirIterator it(
        QStringLiteral(MIACODE_SOURCE_ROOT "/src/app/ui"),
        {QStringLiteral("*.qml")},
        QDir::Files,
        QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QFile file(it.next());
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QString source = QString::fromUtf8(file.readAll());
        auto match = pattern.globalMatch(source);
        while (match.hasNext()) {
            const QString name = match.next().captured(1);
            bool known = false;
            const int value = meta.keyToValue(name.toUtf8().constData(), &known);
            if (!known) {
                continue;
            }
            keys.append({it.fileName(),
                         name,
                         QKeySequence(static_cast<QKeySequence::StandardKey>(value))});
        }
    }
    return keys;
}

void checkTimelineShortcuts(QTextStream& out, int* failed)
{
    QTemporaryDir storage;
    miacode::ui::ShortcutRegistry registry(storage.filePath(QStringLiteral("shortcuts.json")));
    miacode::ui::ShortcutModel shortcuts(registry);
    shortcuts.setShortcutText(QStringLiteral("transform.rotate_ccw_45"), QStringLiteral("Ctrl+Alt+9"));
    shortcuts.setShortcutText(QStringLiteral("transform.rotate_cw_45"), QStringLiteral("Ctrl+Alt+0"));
    shortcuts.setShortcutText(QStringLiteral("timeline.zoom_in"), QStringLiteral("Ctrl+'"));
    shortcuts.setShortcutText(QStringLiteral("timeline.zoom_out"), QStringLiteral("Ctrl+;"));

    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("testShortcuts"), &shortcuts);
    QQmlComponent component(&engine);
    component.setData(QByteArray(R"QML(
import QtQuick
import "file:MIACODE_CHROME_DIR" as Chrome
Window {
    width: 320; height: 200; visible: true
    property alias timeline: timelinePort
    property alias shortcutBindings: bindings
    QtObject {
        id: timelinePort
        property int steps: 0
        property var zoomInGestures: []
        property var zoomOutGestures: []
        function stepZoomIn() { steps++ }
        function stepZoomOut() { steps-- }
        function setZoomWheelShortcuts(zoomIn, zoomOut) {
            zoomInGestures = zoomIn
            zoomOutGestures = zoomOut
        }
    }
    QtObject { id: commands; function shortcutCommandIds() { return [] } }
    QtObject {
        id: preview
        function stopOrPlay() {}
        function togglePlayback() {}
        function adjustRate(direction) {}
    }
    TextInput { id: editor; objectName: "editor"; width: 200; height: 30; focus: true }
    Item { id: other; objectName: "other"; width: 20; height: 20 }
    Chrome.ShortcutBindings {
        id: bindings
        shortcuts: testShortcuts; commands: commands; previewSession: preview
    }
}
)QML").replace("MIACODE_CHROME_DIR", MIACODE_SOURCE_ROOT "/src/app/ui/chrome"), QUrl());
    QScopedPointer<QObject> object(component.create());
    auto* window = qobject_cast<QQuickWindow*>(object.data());
    if (!expect(window != nullptr, window ? QStringLiteral("timeline shortcut fixture loads")
                                          : component.errorString(), out, failed)) return;
    QObject* timeline = window->property("timeline").value<QObject*>();
    QObject* bindings = window->property("shortcutBindings").value<QObject*>();
    // The old component has no target property: key presses still exercise its
    // production bindings and expose the missing command route.
    bindings->setProperty("timelineStateBridge", QVariant::fromValue(timeline));
    window->requestActivate();
    QTest::qWait(100);
    const auto press = [&](Qt::Key key) {
        QTest::keyClick(window, key, Qt::ControlModifier);
        QCoreApplication::processEvents();
    };
    press(Qt::Key_Apostrophe);
    expect(timeline->property("steps").toInt() == 1,
           QStringLiteral("custom timeline zoom in fires with editor focus"), out, failed);
    window->findChild<QQuickItem*>(QStringLiteral("other"))->forceActiveFocus();
    timeline->setProperty("steps", 5);
    press(Qt::Key_Semicolon);
    expect(timeline->property("steps").toInt() == 4,
           QStringLiteral("custom timeline zoom out fires outside editor focus"), out, failed);

    bindings->setProperty("chartCommandsEnabled", false);
    press(Qt::Key_Apostrophe);
    expect(timeline->property("steps").toInt() == 4,
           QStringLiteral("timeline zoom stays inactive outside chart editing"), out, failed);
    bindings->setProperty("chartCommandsEnabled", true);
    shortcuts.setCaptureActive(true);
    press(Qt::Key_Apostrophe);
    expect(timeline->property("steps").toInt() == 4,
           QStringLiteral("recording a shortcut does not zoom the timeline"), out, failed);
    shortcuts.setCaptureActive(false);

    shortcuts.setShortcutText(QStringLiteral("timeline.zoom_in"), QStringLiteral("Ctrl+K"));
    QCoreApplication::processEvents();
    timeline->setProperty("steps", 0);
    press(Qt::Key_Apostrophe);
    press(Qt::Key_K);
    expect(timeline->property("steps").toInt() == 1,
           QStringLiteral("timeline keyboard rebinding takes effect without restart"), out, failed);
    shortcuts.setShortcutText(QStringLiteral("timeline.zoom_in"), QStringLiteral("Alt+WheelUp"));
    shortcuts.setShortcutText(QStringLiteral("timeline.zoom_out"), QStringLiteral("Alt+WheelDown"));
    QCoreApplication::processEvents();
    const auto gestures = [&](const char* name) {
        return timeline->property(name).value<QJSValue>().toVariant().toStringList();
    };
    expect(gestures("zoomInGestures") == QStringList{QStringLiteral("Alt+WheelUp")}
               && gestures("zoomOutGestures") == QStringList{QStringLiteral("Alt+WheelDown")},
           QStringLiteral("wheel gestures synchronize immediately after shortcut edits"), out, failed);
    timeline->setProperty("steps", 0);
    press(Qt::Key_K);
    expect(timeline->property("steps").toInt() == 0,
           QStringLiteral("switching to a wheel gesture releases the old keyboard binding"), out, failed);
    shortcuts.resetShortcut(QStringLiteral("timeline.zoom_in"));
    shortcuts.resetShortcut(QStringLiteral("timeline.zoom_out"));
    QCoreApplication::processEvents();
    expect(gestures("zoomInGestures") == QStringList{QStringLiteral("Ctrl+WheelUp")}
               && gestures("zoomOutGestures") == QStringList{QStringLiteral("Ctrl+WheelDown")},
           QStringLiteral("resetting shortcuts immediately restores default wheel gestures"), out, failed);
}
} // namespace

// File/edit menu keys are fixed: no editable command may take them, by default or by recording.
void checkReservedShortcuts(QTextStream& out, int* failed)
{
    QTemporaryDir storage;
    miacode::ui::ShortcutRegistry registry(storage.filePath(QStringLiteral("shortcuts.json")));
    miacode::ui::ShortcutModel shortcuts(registry);
    const QString stop = QStringLiteral("preview.stop_or_play");
    const QString before = registry.shortcutText(stop);
    expect(shortcuts.reservedShortcutOwner(QStringLiteral("Ctrl+C")) == "action.copy"
               && shortcuts.reservedShortcutOwner(QStringLiteral("Ctrl+S")) == "action.save"
               && shortcuts.reservedShortcutOwner(QStringLiteral("Ctrl+Shift+N")) == "action.new"
               && shortcuts.reservedShortcutOwner(QStringLiteral("Ctrl+K")).isEmpty(),
           QStringLiteral("file and edit keys report the menu command that owns them"), out, failed);
    expect(shortcuts.nativeText(QStringLiteral("Ctrl+C"))
               == QKeySequence(QStringLiteral("Ctrl+C")).toString(QKeySequence::NativeText),
           QStringLiteral("notices spell a recorded key the way the platform shows it"), out, failed);
    expect(!shortcuts.setShortcutText(stop, QStringLiteral("Ctrl+C")) && registry.shortcutText(stop) == before,
           QStringLiteral("recording a reserved key leaves the command unchanged"), out, failed);
    QStringList clashing;
    for (const auto& definition : registry.editableShortcuts()) {
        if (!shortcuts.reservedShortcutOwner(registry.shortcutText(definition.id)).isEmpty())
            clashing.append(definition.id);
    }
    expect(clashing.isEmpty(), QStringLiteral("no default binding takes a reserved key: %1")
               .arg(clashing.join(QStringLiteral(", "))), out, failed);
}

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    Q_UNUSED(app);
    QTextStream out(stdout);
    int failed = 0;

    checkPendingInputCommit(out, &failed);
    checkInputOwnership(out, &failed);
    checkPreviewWindowOwnership(out, &failed);
    checkTimelineShortcuts(out, &failed);
    checkReservedShortcuts(out, &failed);

    miacode::ui::ShortcutModel model;
    for (int flags = 0; flags < 8; ++flags) {
        expect(miacode::runtime::previewStopOrPlayShouldStop(flags & 1, flags & 2, flags & 4)
                   == (flags != 0),
               QStringLiteral("Stop or Play stops active or pending startup states (%1)").arg(flags), out, &failed);
    }
    const QStringList boundIds = miacode::ui::shortcutCommandIds();

    // A declared command can intentionally have no default binding. Identity,
    // rather than a nonempty sequence, determines whether it is configurable.
    QSet<QString> editableIds;
    for (const auto& definition : miacode::ui::ShortcutRegistry::instance().editableShortcuts()) {
        editableIds.insert(definition.id);
    }
    QStringList unknown;
    for (const QString& id : boundIds) {
        if (!editableIds.contains(id)) unknown.append(id);
    }
    if (!unknown.isEmpty()) out << "  unknown ids: " << unknown.join(QStringLiteral(", ")) << '\n';
    expect(unknown.isEmpty(),
           QStringLiteral("every bound command id resolves to an editable registry definition"), out, &failed);

    for (const QString& id : {QStringLiteral("transform.clear_complete_elements"),
                              QStringLiteral("transform.reset_tap_notes")}) {
        expect(editableIds.contains(id) && boundIds.contains(id)
                   && miacode::ui::ShortcutRegistry::instance().defaultShortcutText(id).isEmpty(),
               QStringLiteral("%1 remains configurable and dispatched without assigning a default binding").arg(id),
               out, &failed);
    }

    const QSet<QString> uniqueIds(boundIds.cbegin(), boundIds.cend());
    expect(!boundIds.isEmpty() && boundIds.size() == uniqueIds.size(),
           QStringLiteral("the bound command table has no duplicate ids"), out, &failed);

    // Portable text is what QML's Shortcut.sequence parses; native text is what
    // a menu row shows. They are different strings on macOS and must not be
    // swapped at the call site.
    const QString portable = model.sequence(QStringLiteral("transform.mirror_lr"));
    const QString native = model.displayText(QStringLiteral("transform.mirror_lr"));
    expect(!portable.isEmpty() && !native.isEmpty()
               && portable == QKeySequence(portable, QKeySequence::PortableText)
                                  .toString(QKeySequence::PortableText),
           QStringLiteral("a binding is exposed in both portable and display spellings"), out, &failed);

    // An unknown id falls back to what the caller supplied rather than binding
    // something arbitrary, and stays empty when nothing was supplied.
    expect(model.sequence(QStringLiteral("no.such.command")).isEmpty()
               && model.sequence(QStringLiteral("no.such.command"), QStringLiteral("Ctrl+B"))
                      == QStringLiteral("Ctrl+B"),
           QStringLiteral("an unknown id stays inert unless the caller supplies a fallback"),
           out, &failed);

    expect(!model.standardDisplayText(static_cast<int>(QKeySequence::Save)).isEmpty(),
           QStringLiteral("standard-key menu rows can render their platform spelling"), out, &failed);

    // The bound ids and the dispatch table are now derived from one another, so
    // an id without a handler is unrepresentable — what can still drift is the
    // registry's hand-written editable-id list. A transform the registry offers
    // for rebinding but the table does not know is a shortcut the user can
    // customize and never fire; one the table knows but the registry does not
    // is a command with no configurable binding.
    QSet<QString> registryTransformIds;
    for (const auto& definition : miacode::ui::ShortcutRegistry::instance().editableShortcuts()) {
        if (definition.id.startsWith(QStringLiteral("transform."))) {
            registryTransformIds.insert(definition.id);
        }
    }
    const QSet<QString> tableIds(boundIds.cbegin(), boundIds.cend());
    const QStringList registryOnly = QStringList(
        (registryTransformIds - tableIds).values());
    const QStringList tableOnly = QStringList((tableIds - registryTransformIds).values());
    if (!registryOnly.isEmpty()) out << "  rebindable but undispatched: " << registryOnly.join(QStringLiteral(", ")) << '\n';
    if (!tableOnly.isEmpty()) out << "  dispatched but not rebindable: " << tableOnly.join(QStringLiteral(", ")) << '\n';
    expect(!registryTransformIds.isEmpty() && registryOnly.isEmpty() && tableOnly.isEmpty(),
           QStringLiteral("the rebindable transform ids and the dispatch table describe the same set"), out, &failed);

    // And the table dispatches to a real transform, not an empty slot: a mirror
    // over one bar renumbers every button.
    int mirrored = 0;
    const QString sample = QStringLiteral("{8}1,2,3,4,");
    QString mirrorResult;
    for (const miacode::ui::ChartTransformSpec& spec : miacode::ui::chartTransformSpecs()) {
        if (spec.id == QStringLiteral("transform.mirror_lr") && spec.apply) {
            mirrorResult = spec.apply(sample, {}, &mirrored);
        }
    }
    expect(mirrored > 0 && mirrorResult != sample,
           QStringLiteral("a table entry reaches the shared chart transform"), out, &failed);

    // A menu row bound to a QKeySequence::StandardKey registers a window-context
    // shortcut of its own. When that lands on a sequence the registry also hands
    // to a Shortcut in ShortcutBindings.qml, Qt matches both and dispatches
    // neither — it reports the pair as ambiguous — so BOTH commands go dead.
    // Porting v1's File menu to standard keys is what silently took Ctrl+O
    // (播放速度降低) out of v2: v1 spelled 打开 as Ctrl+Shift+O and left Ctrl+O
    // to the preview.
    QStringList clashes;
    for (const MenuStandardKey& key : menuStandardKeys()) {
        for (const auto& definition : miacode::ui::ShortcutRegistry::instance().editableShortcuts()) {
            const QKeySequence bound = miacode::ui::ShortcutRegistry::instance().sequence(definition.id);
            if (!bound.isEmpty() && bound == key.sequence) {
                clashes.append(QStringLiteral("%1 StandardKey.%2 (%3) == %4")
                                   .arg(key.file,
                                        key.keyName,
                                        bound.toString(QKeySequence::PortableText),
                                        definition.id));
            }
        }
    }
    if (!clashes.isEmpty()) {
        for (const QString& clash : clashes) out << "  ambiguous: " << clash << '\n';
    }
    expect(clashes.isEmpty(),
           QStringLiteral("no menu standard key collides with a rebindable command"),
           out, &failed);

    // ⌘⇧= on a US or 拼音 layout reaches Qt as both Ctrl+Shift+= (the key plus
    // Shift) and Ctrl++ (the character Shift made), and two bindings matched by
    // one press are reported as ambiguous and fire nothing. Ctrl++ alone is
    // reached by that press, by an unshifted + key (German and similar
    // layouts) and by the keypad +, so it is the half-step default and
    // Ctrl+Shift+= must not come back as anyone's.
    const auto& registry = miacode::ui::ShortcutRegistry::instance();
    expect(editableIds.contains(QStringLiteral("ui.zoom_in"))
               && editableIds.contains(QStringLiteral("ui.zoom_out"))
               && editableIds.contains(QStringLiteral("ui.zoom_reset"))
               && registry.defaultShortcutText(QStringLiteral("ui.zoom_in")) == QStringLiteral("Ctrl+Alt+=")
               && registry.defaultShortcutText(QStringLiteral("ui.zoom_out")) == QStringLiteral("Ctrl+Alt+-")
               && registry.defaultShortcutText(QStringLiteral("ui.zoom_reset")) == QStringLiteral("Ctrl+Alt+0")
               && !editableIds.contains(QStringLiteral("editor.font_increase"))
               && !editableIds.contains(QStringLiteral("editor.font_decrease"))
               && model.sequence(QStringLiteral("editor.font_increase")).isEmpty()
               && model.sequence(QStringLiteral("editor.font_decrease")).isEmpty(),
           QStringLiteral("UI scale commands replace editor font shortcuts without leaving duplicate legacy bindings"),
           out, &failed);
    QStringList shiftedEqualsDefaults;
    for (const auto& definition : registry.editableShortcuts()) {
        if (registry.defaultShortcutText(definition.id) == QStringLiteral("Ctrl+Shift+=")) {
            shiftedEqualsDefaults.append(definition.id);
        }
    }
    if (!shiftedEqualsDefaults.isEmpty()) {
        out << "  Ctrl+Shift+= defaults: " << shiftedEqualsDefaults.join(QStringLiteral(", ")) << '\n';
    }
    expect(registry.defaultShortcutText(QStringLiteral("transform.subdivision_half_up"))
                   == QStringLiteral("Ctrl++")
               && shiftedEqualsDefaults.isEmpty(),
           QStringLiteral("subdivision +1/2 defaults to Ctrl++ and no default also claims its Ctrl+Shift+= twin"),
           out, &failed);

    expect(registry.defaultShortcutText(QStringLiteral("preview.stop_or_play"))
                   == QStringLiteral("Ctrl+Shift+C")
               && registry.defaultShortcutText(QStringLiteral("preview.play_pause_global"))
                   == QStringLiteral("Ctrl+Shift+X"),
           QStringLiteral("preview shortcut defaults match the v1 C and X bindings"), out, &failed);

    {
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(QByteArray(R"QML(
import QtQuick
import QtQuick.Controls
import "file:MIACODE_CHROME_DIR" as Chrome
Window {
    width: 320; height: 200; visible: true
    property int starts: 0
    property int toggles: 0
    property bool playbackAllowed: false
    property QtObject mainView: QtObject {
        property bool chartEditorActive: false
        property QtObject pages: QtObject {
            property QtObject exportSession: QtObject {
                property bool pageSessionActive: true
                property int selectedDifficultyId: 4
            }
        }
    }
    QtObject {
        id: preview
        property bool playing: false
        function stopOrPlay() { starts++ }
        function togglePlayback() { toggles++ }
        function stop() {}
        function adjustRate(direction) {}
    }
    QtObject {
        id: bindings
        property int revision: 1
        property string cKey: "Ctrl+Shift+C"
        property string xKey: "Ctrl+Shift+X"
        function sequence(id, fallback) {
            if (id === "preview.stop_or_play") return cKey
            if (id === "preview.play_pause_global") return xKey
            return ""
        }
    }
    QtObject { id: commandSource; function shortcutCommandIds() { return [] } }
    TextInput { anchors.fill: parent; focus: true }
    Chrome.ShortcutBindings {
        shortcuts: bindings; commands: commandSource; previewSession: preview
        chartCommandsEnabled: false
        playbackCommandsEnabled: playbackAllowed
    }
    function rebind() { bindings.cKey = "Ctrl+Alt+J"; bindings.xKey = "Ctrl+Alt+K"; bindings.revision++ }
}
)QML").replace("MIACODE_CHROME_DIR", MIACODE_SOURCE_ROOT "/src/app/ui/chrome"), QUrl());
        QScopedPointer<QObject> object(component.create());
        auto* window = qobject_cast<QQuickWindow*>(object.data());
        expect(window != nullptr,
               window ? QStringLiteral("preview shortcut QML fixture loads") : component.errorString(),
               out, &failed);
        if (window) {
            QFile mainFile(QStringLiteral(MIACODE_SOURCE_ROOT "/src/app/ui/Main.qml"));
            const QString mainSource = mainFile.open(QIODevice::ReadOnly)
                ? QString::fromUtf8(mainFile.readAll()) : QString();
            const QRegularExpression gatePattern(
                QStringLiteral("readonly property bool playbackCommandsEnabled:([\\s\\S]*?)\\n    readonly property"));
            const QString gate = gatePattern.match(mainSource).captured(1);
            QQmlExpression eligibility(QQmlEngine::contextForObject(window), window, gate);
            expect(!gate.isEmpty(), QStringLiteral("Main playback eligibility expression is present"), out, &failed);
            const QVariant exportEnabled = eligibility.evaluate();
            if (eligibility.hasError()) out << "  eligibility error: " << eligibility.error().toString() << '\n';
            expect(!gate.isEmpty() && exportEnabled.toBool() && !eligibility.hasError(),
                   QStringLiteral("Main enables playback for export audition without an active chart"), out, &failed);
            QObject* mainView = window->property("mainView").value<QObject*>();
            QObject* pages = mainView->property("pages").value<QObject*>();
            QObject* exportSession = pages->property("exportSession").value<QObject*>();
            exportSession->setProperty("selectedDifficultyId", 0);
            expect(!eligibility.evaluate().toBool(),
                   QStringLiteral("export playback requires a selected difficulty"), out, &failed);
            exportSession->setProperty("selectedDifficultyId", 4);
            exportSession->setProperty("pageSessionActive", false);
            expect(!eligibility.evaluate().toBool(),
                   QStringLiteral("inactive export pages do not enable preview shortcuts"), out, &failed);
            mainView->setProperty("chartEditorActive", true);
            expect(eligibility.evaluate().toBool(),
                   QStringLiteral("chart editing continues to enable preview shortcuts"), out, &failed);
            mainView->setProperty("chartEditorActive", false);
            expect(pages->setProperty("exportSession", QVariant::fromValue<QObject*>(nullptr)),
                   QStringLiteral("export session fixture accepts withdrawal"), out, &failed);
            expect(!eligibility.evaluate().toBool() && !eligibility.hasError(),
                   QStringLiteral("a withdrawn export session leaves shortcuts disabled"), out, &failed);
            expect(pages->setProperty("exportSession", QVariant::fromValue(exportSession)),
                   QStringLiteral("export session fixture accepts restoration"), out, &failed);
            exportSession->setProperty("pageSessionActive", true);
            window->setProperty("playbackAllowed", eligibility.evaluate().toBool());
            window->requestActivate();
            QTest::qWait(100);
            expect(window->activeFocusItem()
                       && window->activeFocusItem()->inherits("QQuickTextInput"),
                   QStringLiteral("shortcut test keeps a real text editor focused"), out, &failed);
            const auto press = [window](int key, Qt::KeyboardModifiers mods) {
                QTest::keyClick(window, static_cast<Qt::Key>(key), mods);
                QCoreApplication::processEvents();
            };
            press(Qt::Key_C, Qt::ControlModifier | Qt::ShiftModifier);
            press(Qt::Key_X, Qt::ControlModifier | Qt::ShiftModifier);
            expect(window->property("starts").toInt() == 1 && window->property("toggles").toInt() == 1,
                   QStringLiteral("both preview commands fire with editor focus and no chart commands"), out, &failed);
            QMetaObject::invokeMethod(window, "rebind");
            QCoreApplication::processEvents();
            press(Qt::Key_C, Qt::ControlModifier | Qt::ShiftModifier);
            press(Qt::Key_X, Qt::ControlModifier | Qt::ShiftModifier);
            press(Qt::Key_J, Qt::ControlModifier | Qt::AltModifier);
            press(Qt::Key_K, Qt::ControlModifier | Qt::AltModifier);
            expect(window->property("starts").toInt() == 2 && window->property("toggles").toInt() == 2,
                   QStringLiteral("preview bindings follow revised sequences and release their previous keys"), out, &failed);
        }
    }

    if (failed != 0) {
        out << "QmlShortcutBinding spec failed: " << failed << '\n';
        return 1;
    }
    out << "QmlShortcutBinding spec passed.\n";
    return 0;
}
