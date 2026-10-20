#include <QEventLoop>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QTextStream>
#include <QVariant>

#include <memory>

#ifndef MIACODE_QML_SPEC_IMPORT_ROOT
#error "MIACODE_QML_SPEC_IMPORT_ROOT must be defined"
#endif

namespace {

bool expect(bool condition, const QString& message, QTextStream& out, int* failed)
{
    out << (condition ? "[PASS] " : "[FAIL] ") << message << '\n';
    out.flush();
    if (!condition) ++*failed;
    return condition;
}

// The real PreviewRateMenu / PreviewRateToast over a stand-in session that owns
// nothing but the rate, so the specs describe the QML's own behaviour rather
// than the runtime's.
std::unique_ptr<QObject> createHarness(QQmlEngine& engine, QTextStream& out)
{
    QQmlComponent component(&engine);
    component.setData(R"QML(
        import QtQuick
        import QtQuick.Controls
        import MiaCode.UI

        ApplicationWindow {
            id: harness
            visible: true
            width: 640
            height: 480

            QtObject {
                id: session
                property real rate: 1
            }

            readonly property real sessionRate: session.rate
            readonly property bool toastShowing: toast.showing
            readonly property int toastPercent: toast.percent
            readonly property int rowCount: rateMenu.count

            // What a click does to a checkable MenuItem: AbstractButton toggles
            // first, then the row emits triggered().
            function pick(index) {
                const item = rateMenu.itemAt(index)
                item.toggle()
                item.triggered()
            }
            function checkedCount() {
                let total = 0
                for (let i = 0; i < rateMenu.count; i++)
                    if (rateMenu.itemAt(i).checked) total++
                return total
            }
            function checkedIndex() {
                for (let i = 0; i < rateMenu.count; i++)
                    if (rateMenu.itemAt(i).checked) return i
                return -1
            }
            function rateAt(index) { return rateMenu.rates[index] }
            function setRate(value) { session.rate = value }

            PreviewRateMenu {
                id: rateMenu
                previewSession: session
            }

            PreviewRateToast {
                id: toast
                width: 400
                height: 300
                previewSession: session
            }
        }
    )QML",
                       QUrl(QStringLiteral("qrc:/spec/PreviewRateHarness.qml")));
    std::unique_ptr<QObject> root(component.create());
    if (root == nullptr) {
        out << "  " << component.errorString() << '\n';
    }
    return root;
}

int checkedCount(QObject* harness)
{
    QVariant result;
    QMetaObject::invokeMethod(harness, "checkedCount", Q_RETURN_ARG(QVariant, result));
    return result.toInt();
}

int checkedIndex(QObject* harness)
{
    QVariant result;
    QMetaObject::invokeMethod(harness, "checkedIndex", Q_RETURN_ARG(QVariant, result));
    return result.toInt();
}

void pick(QObject* harness, int index)
{
    QMetaObject::invokeMethod(harness, "pick", Q_ARG(QVariant, QVariant(index)));
}

double rateAt(QObject* harness, int index)
{
    QVariant result;
    QMetaObject::invokeMethod(
        harness, "rateAt", Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, QVariant(index)));
    return result.toDouble();
}

bool verifyMenuAndTheme(QQmlEngine& engine, QTextStream& out, int* failed)
{
    QQmlComponent component(&engine);
    component.setData(R"QML(
import QtQuick
import QtQuick.Controls
import MiaCode.UI
Item {
    id: root
    property string selectedCommand: ""
    readonly property string paletteToken: Theme.activeThemeToken
    readonly property color panelColor: Theme.colors.background.panel
    QtObject {
        id: preferences
        property bool darkTheme: true
        property bool blurMaterialsEnabled: false
        property string activeThemeToken: "dark"
    }
    QtObject {
        id: actions
        property bool canTransform: true
        property bool canNormalize: true
        signal chartTransformRequested(string commandId)
        onChartTransformRequested: function(commandId) { root.selectedCommand = commandId }
    }
    QtObject {
        id: shortcutModel
        property int revision: 0
        function sequence(id, fallback) { return fallback || "Ctrl+J" }
        function displayText(id, fallback) { return fallback || "Ctrl+J" }
        function standardDisplayText(key) { return "" }
    }
    QtObject {
        id: document
        function chartTransformMenu() {
            return [{id: "transform.mirror_lr", labelKey: "mirror", section: 0}]
        }
    }
    QtObject { id: petModel; property bool visible: false }
    MainMenu {
        id: menu
        commands: actions
        shortcuts: shortcutModel
        documentSession: document
        pet: petModel
    }
    function triggerTransform() {
        const adjust = menu.nativeMenus()[2]
        for (let i = 0; i < adjust.count; ++i) {
            const item = adjust.itemAt(i)
            if (item && item.objectName === "adjustTransform_transform.mirror_lr") {
                item.click()
                return item.enabled
            }
        }
        return false
    }
    function selectLight() {
        preferences.darkTheme = false
        preferences.activeThemeToken = "light"
    }
    Component.onCompleted: Theme.preferences = preferences
    Component.onDestruction: Theme.preferences = null
}
)QML", QUrl(QStringLiteral("qrc:/spec/MenuThemeHarness.qml")));
    std::unique_ptr<QObject> harness(component.create());
    if (!harness) {
        for (const auto& error : component.errors()) out << error.toString() << Qt::endl;
        return expect(false, QStringLiteral("menu and theme instantiate"), out, failed);
    }
    QVariant triggered;
    QMetaObject::invokeMethod(harness.get(), "triggerTransform", Q_RETURN_ARG(QVariant, triggered));
    bool ok = expect(triggered.toBool()
                         && harness->property("selectedCommand").toString() == QStringLiteral("transform.mirror_lr"),
                     QStringLiteral("transform menu row dispatches its command"), out, failed);
    const QVariant darkPanel = harness->property("panelColor");
    QMetaObject::invokeMethod(harness.get(), "selectLight");
    ok &= expect(harness->property("paletteToken").toString() == QStringLiteral("light")
                     && harness->property("panelColor") != darkPanel,
                 QStringLiteral("theme selection updates the bound palette"), out, failed);
    return ok;
}

} // namespace

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QGuiApplication app(argc, argv);
    QTextStream out(stdout);
    int failed = 0;

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(MIACODE_QML_SPEC_IMPORT_ROOT));
    if (!verifyMenuAndTheme(engine, out, &failed)) return 1;
    std::unique_ptr<QObject> harness = createHarness(engine, out);
    if (!expect(harness != nullptr, QStringLiteral("the rate menu and HUD instantiate"), out, &failed)) {
        return 1;
    }
    QObject* root = harness.get();

    // The HUD announces changes, not the rate the session already had when the
    // pane was built — otherwise it flashes on every page switch and on every
    // entry into fullscreen.
    expect(!root->property("toastShowing").toBool(),
           QStringLiteral("the HUD stays down for the rate the session was built with"),
           out, &failed);

    const int rowCount = root->property("rowCount").toInt();
    expect(rowCount > 0, QStringLiteral("the rate menu builds a row per preset"), out, &failed);

    // The ladder the menu offers must be the one the backend steps through, or
    // Ctrl+O walks the session onto a rate no row can report.
    const QList<double> backendLadder{0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 2.0};
    bool ladderMatches = rowCount == backendLadder.size();
    for (int i = 0; ladderMatches && i < rowCount; ++i) {
        ladderMatches = qFuzzyCompare(rateAt(root, i), backendLadder.at(i));
    }
    expect(ladderMatches,
           QStringLiteral("the menu ladder matches kPreviewPlaybackRateOptions"), out, &failed);

    expect(checkedCount(root) == 1 && qFuzzyCompare(rateAt(root, checkedIndex(root)), 1.0),
           QStringLiteral("exactly the session's rate starts ticked"), out, &failed);

    // The reported bug: a second pick used to leave the first one ticked,
    // because AbstractButton.toggle() had already torn the row's binding down.
    pick(root, rowCount - 1);
    expect(checkedCount(root) == 1 && checkedIndex(root) == rowCount - 1,
           QStringLiteral("picking a rate ticks that row alone"), out, &failed);

    pick(root, 1);
    expect(checkedCount(root) == 1 && checkedIndex(root) == 1,
           QStringLiteral("picking a second rate does not leave the first ticked"), out, &failed);

    pick(root, 1);
    expect(checkedCount(root) == 1 && checkedIndex(root) == 1,
           QStringLiteral("re-picking the current rate keeps it ticked"), out, &failed);

    expect(qFuzzyCompare(root->property("sessionRate").toDouble(), rateAt(root, 1)),
           QStringLiteral("a pick reaches the session"), out, &failed);

    // A rate the user never picked — the speed shortcut, the Preview menu —
    // still has to move the tick, which only holds while the rows stay bound.
    QMetaObject::invokeMethod(root, "setRate", Q_ARG(QVariant, QVariant(1.5)));
    expect(checkedCount(root) == 1 && qFuzzyCompare(rateAt(root, checkedIndex(root)), 1.5),
           QStringLiteral("a rate set outside the menu moves the tick"), out, &failed);

    QMetaObject::invokeMethod(root, "setRate", Q_ARG(QVariant, QVariant(0.75)));
    expect(root->property("toastShowing").toBool()
               && root->property("toastPercent").toInt() == 75,
           QStringLiteral("a rate change raises the HUD with that rate"), out, &failed);

    // And it lets go on its own — a HUD that stays up is worse than none.
    QEventLoop loop;
    QTimer::singleShot(1400, &loop, &QEventLoop::quit);
    loop.exec();
    expect(!root->property("toastShowing").toBool(),
           QStringLiteral("the HUD retires itself after the hold"), out, &failed);

    if (failed != 0) {
        out << "QmlPreviewRate spec failed: " << failed << '\n';
        return 1;
    }
    out << "QmlPreviewRate spec passed.\n";
    return 0;
}
