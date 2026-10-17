#include "app/ui/document/ChartTransformCommands.h"
#include "app/ui/chrome/ShortcutCommands.h"
#include "app/ui/chrome/ShortcutModel.h"
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
} // namespace

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    Q_UNUSED(app);
    QTextStream out(stdout);
    int failed = 0;

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
