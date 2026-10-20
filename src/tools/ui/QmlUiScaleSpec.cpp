#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTest>
#include <QTextStream>
#include <QVariant>

#include <cmath>
#include <memory>
#include <utility>

#ifndef MIACODE_QML_SPEC_IMPORT_ROOT
#error "MIACODE_QML_SPEC_IMPORT_ROOT must be defined"
#endif

namespace {

bool near(qreal actual, qreal expected)
{
    return std::abs(actual - expected) <= 0.01;
}

bool expect(bool condition, const QString& message, QTextStream& out, int* failed)
{
    out << (condition ? "[PASS] " : "[FAIL] ") << message << '\n';
    out.flush();
    if (!condition) ++*failed;
    return condition;
}

std::unique_ptr<QObject> createHarness(QQmlEngine& engine, QTextStream& out)
{
    QQmlComponent component(&engine);
    component.setData(R"QML(
        import QtQuick
        import QtQuick.Controls
        import MiaCode.UI

        Item {
            id: harness
            property QtObject preferences: QtObject {
                property int uiScalePercent: 100
                property int fontSize: 13
                property string uiFontFamily: "Sans Serif"
                property font codeFont: Qt.font({ family: "monospace", pixelSize: 13 })
                property int editorBlockSpacing: 0
                property bool blurMaterialsEnabled: false
                property bool darkTheme: true
                property string activeThemeToken: "dark"
            }
            property QtObject shortcuts: QtObject {
                property int revision: 1
                property bool captureActive: false
                function sequence(id, fallback) { return fallback }
            }
            Component.onCompleted: Theme.preferences = harness.preferences

            component ScaleWindow: ApplicationWindow {
                id: host
                transientParent: null
                visible: true
                property int clickCount: 0
                property int closeCount: 0
                readonly property Item overlayItem: surface.overlayItem
                readonly property Item dialogVisual: dialog.background.parent
                readonly property Item comboVisual: combo.popup.background.parent
                function openDialog() { dialog.open() }
                function closeDialog() { dialog.close() }
                function openCombo() { combo.popup.open() }
                function closeCombo() { combo.popup.close() }

                UiScaleShortcuts {
                    shortcuts: harness.shortcuts
                    preferencesModel: harness.preferences
                }

                UiScaleSurface {
                    id: surface
                    objectName: "scaleSurface"

                    AppButton {
                        id: action
                        objectName: "scaleButton"
                        x: 20
                        y: 20
                        width: 140
                        text: "Scale button"
                        onClicked: host.clickCount++
                    }
                    Text {
                        objectName: "scaleText"
                        x: 20
                        y: 75
                        text: "Fixed caption"
                        font.pixelSize: 11
                    }
                    Image {
                        objectName: "scaleIcon"
                        x: 180
                        y: 25
                        width: 16
                        height: 16
                        source: "data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='16' height='16'%3E%3Crect width='16' height='16' fill='black'/%3E%3C/svg%3E"
                    }
                    ActivityBar {
                        objectName: "scaleActivityBar"
                        x: 20
                        y: 110
                        height: 180
                    }
                    AppTab {
                        objectName: "scaleTab"
                        x: 75
                        y: 110
                        width: 185
                        text: "Master 13+"
                        difficultyId: 5
                        active: true
                        closable: true
                        onCloseRequested: host.closeCount++
                    }
                    AppComboBox {
                        id: combo
                        objectName: "scaleCombo"
                        x: surface.width - width - 8
                        y: surface.height - height - 8
                        width: 120
                        model: ["Small", "Medium", "Large"]
                    }
                    AppDialog {
                        id: dialog
                        preferredWidth: 440
                        preferredHeight: 280
                        title: "Scale dialog"
                        header: null
                        footer: null
                        contentItem: Item {
                            AppButton {
                                objectName: "dialogButton"
                                anchors.centerIn: parent
                                text: "Dialog action"
                                onClicked: host.clickCount++
                            }
                        }
                    }
                }
            }
            ScaleWindow { objectName: "firstWindow"; width: 900; height: 680 }
            ScaleWindow { objectName: "secondWindow"; width: 700; height: 500 }
        }
    )QML", QUrl(QStringLiteral("qrc:/spec/UiScaleHarness.qml")));
    std::unique_ptr<QObject> root(component.create());
    if (root == nullptr) out << component.errorString() << '\n';
    return root;
}

QQuickItem* itemProperty(QObject* object, const char* property)
{
    return qobject_cast<QQuickItem*>(object->property(property).value<QObject*>());
}

bool fillsWindow(QQuickItem* item, QQuickWindow* window)
{
    if (item == nullptr) return false;
    const QRectF scene = item->mapRectToScene(QRectF(0, 0, item->width(), item->height()));
    return near(scene.x(), 0) && near(scene.y(), 0)
        && near(scene.width(), window->width()) && near(scene.height(), window->height());
}

bool insideWindow(QQuickItem* item, QQuickWindow* window)
{
    if (item == nullptr || !item->isVisible()) return false;
    const QRectF scene = item->mapRectToScene(QRectF(0, 0, item->width(), item->height()));
    return scene.left() >= -0.01 && scene.top() >= -0.01
        && scene.right() <= window->width() + 0.01
        && scene.bottom() <= window->height() + 0.01;
}

void describeItem(const char* label, QQuickItem* item, QTextStream& out)
{
    if (item == nullptr) {
        out << "  " << label << ": null\n";
        return;
    }
    const QRectF scene = item->mapRectToScene(QRectF(0, 0, item->width(), item->height()));
    QQuickItem* parent = item->parentItem();
    out << "  " << label << " logical pos " << item->x() << ',' << item->y()
        << ", size " << item->width() << 'x' << item->height()
        << ", scale " << item->scale() << ", origin " << item->transformOrigin()
        << ", origin point " << item->transformOriginPoint().x() << ',' << item->transformOriginPoint().y()
        << ", scene " << scene.x() << ',' << scene.y()
        << ' ' << scene.width() << 'x' << scene.height();
    if (parent != nullptr) {
        out << ", parent " << parent->metaObject()->className()
            << " pos " << parent->x() << ',' << parent->y()
            << ", size " << parent->width() << 'x' << parent->height()
            << ", scale " << parent->scale()
            << ", origin " << parent->transformOrigin();
    }
    out << '\n';
}

void settle()
{
    QTest::qWait(30);
}

void prepareControlIcons(QQuickWindow* window)
{
    // The spec import tree mirrors QML without the application's resource
    // icons. A square fixture makes painted size observable on real controls.
    const QUrl source(QStringLiteral("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='22' height='22'%3E%3Crect width='22' height='22' fill='black'/%3E%3C/svg%3E"));
    for (QQuickItem* item : window->findChildren<QQuickItem*>()) {
        if (item->objectName() == QStringLiteral("activityButtonIcon")
            || item->objectName() == QStringLiteral("tabCloseIcon")) {
            item->setProperty("source", source);
        }
    }
}

void checkControlIcons(QQuickWindow* window, qreal factor, const QString& prefix,
                       QTextStream& out, int* failed)
{
    QQuickItem* activity = window->findChild<QQuickItem*>(QStringLiteral("scaleActivityBar"));
    QQuickItem* tab = window->findChild<QQuickItem*>(QStringLiteral("scaleTab"));
    if (!expect(activity != nullptr && tab != nullptr,
                prefix + QStringLiteral("real activity and editor-tab controls exist"), out, failed)) return;
    for (const auto& fixture : {std::pair<QQuickItem*, qreal>(activity, 22), {tab, 14}}) {
        const QString name = fixture.first == activity ? QStringLiteral("activityButtonIcon")
                                                      : QStringLiteral("tabCloseIcon");
        QQuickItem* icon = fixture.first->findChild<QQuickItem*>(name);
        if (!expect(icon != nullptr, prefix + name + QStringLiteral(" exists"), out, failed)) continue;
        const qreal baseline = fixture.second;
        const QRectF painted = icon->mapRectToScene(QRectF(
            (icon->width() - icon->property("paintedWidth").toReal()) / 2,
            (icon->height() - icon->property("paintedHeight").toReal()) / 2,
            icon->property("paintedWidth").toReal(), icon->property("paintedHeight").toReal()));
        expect(icon->property("status").toInt() == 1
                   && near(icon->width(), baseline) && near(icon->height(), baseline)
                   && near(painted.width(), baseline * factor)
                   && near(painted.height(), baseline * factor),
               prefix + name + QStringLiteral(" paints at the baseline size scaled exactly once"), out, failed);
    }

    QQuickItem* chrome = nullptr;
    for (QQuickItem* candidate : activity->findChildren<QQuickItem*>(QStringLiteral("activityButtonChrome"))) {
        if (candidate->property("selected").toBool()) {
            chrome = candidate;
            break;
        }
    }
    // All four ActivityButtons use the same object names. QObject ownership
    // order may visit the settings button first, so pair the selected chrome
    // with its own button's content rather than two independent findChilds.
    QQuickItem* activityButton = chrome != nullptr && chrome->parentItem() != nullptr
        ? chrome->parentItem()->parentItem() : nullptr;
    QQuickItem* activityContent = activityButton != nullptr
        ? itemProperty(activityButton, "contentItem") : nullptr;
    QQuickItem* activityIcon = activityContent != nullptr
        ? activityContent->findChild<QQuickItem*>(QStringLiteral("activityButtonIcon")) : nullptr;
    if (expect(chrome != nullptr && !chrome->childItems().isEmpty() && activityIcon != nullptr,
               prefix + QStringLiteral("activity selection chrome exists"), out, failed)) {
        QQuickItem* fill = chrome->childItems().first();
        const QRectF fillScene = fill->mapRectToScene(QRectF(0, 0, fill->width(), fill->height()));
        const QRectF iconScene = activityIcon->mapRectToScene(
            QRectF(0, 0, activityIcon->width(), activityIcon->height()));
        out << "  activity selected=" << chrome->property("selected").toBool()
            << " status=" << activityIcon->property("status").toInt()
            << " painted=" << activityIcon->property("paintedWidth").toReal() << 'x'
            << activityIcon->property("paintedHeight").toReal()
            << " fill=" << fill->width() << 'x' << fill->height()
            << " fillScene=" << fillScene.x() << ',' << fillScene.y() << ' '
            << fillScene.width() << 'x' << fillScene.height()
            << " iconScene=" << iconScene.x() << ',' << iconScene.y() << ' '
            << iconScene.width() << 'x' << iconScene.height() << '\n';
        expect(chrome->property("selected").toBool()
                   && activityIcon->property("status").toInt() == 1
                   && near(activityIcon->property("paintedWidth").toReal(), 22)
                   && near(activityIcon->property("paintedHeight").toReal(), 22)
                   && near(fill->width(), 33) && near(fill->height(), 32)
                   && near(fillScene.width(), 33 * factor) && near(fillScene.height(), 32 * factor)
                   && fillScene.contains(iconScene)
                   && near(fillScene.center().x(), iconScene.center().x())
                   && near(fillScene.center().y(), iconScene.center().y()),
               prefix + QStringLiteral("selected activity glyph stays centered inside linearly scaled chrome"),
               out, failed);
    }
    QQuickItem* close = tab->findChild<QQuickItem*>(QStringLiteral("tabCloseButton"));
    if (!expect(close != nullptr, prefix + QStringLiteral("tab close hit target exists"), out, failed)) return;
    QQuickItem* closeBackground = itemProperty(close, "background");
    expect(closeBackground != nullptr && near(close->width(), 24) && near(close->height(), 24)
               && near(closeBackground->width(), 24) && near(closeBackground->height(), 24),
           prefix + QStringLiteral("tab close keeps its 24px hover and hit area around the 14px glyph"),
           out, failed);
    // Click padding outside the glyph; the whole close-button area must work.
    const QPoint closePoint = close->mapToScene(QPointF(3, close->height() / 2)).toPoint();
    QTest::mouseMove(window, closePoint);
    settle();
    const int closed = window->property("closeCount").toInt();
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, closePoint);
    settle();
    expect(close->property("hovered").toBool()
               && window->property("closeCount").toInt() == closed + 1,
           prefix + QStringLiteral("hover and click still reach the close target outside its glyph"),
           out, failed);
}

void checkWindow(QQuickWindow* window, int percent, QTextStream& out, int* failed)
{
    const qreal factor = percent / 100.0;
    const QString prefix = window->objectName() + QStringLiteral(" at %1%: ").arg(percent);
    checkControlIcons(window, factor, prefix, out, failed);
    QQuickItem* surface = window->findChild<QQuickItem*>(QStringLiteral("scaleSurface"));
    QQuickItem* overlay = itemProperty(window, "overlayItem");
    expect(surface != nullptr && near(surface->width(), window->width() / factor)
               && near(surface->height(), window->height() / factor)
               && fillsWindow(surface, window),
           prefix + QStringLiteral("logical surface maps to the complete physical window"), out, failed);
    if (surface == nullptr || !near(surface->width(), window->width() / factor)
        || !near(surface->height(), window->height() / factor) || !fillsWindow(surface, window)) {
        describeItem("surface", surface, out);
        describeItem("host content item", itemProperty(window, "contentItem"), out);
        describeItem("window root item", window->contentItem(), out);
    }
    expect(overlay != nullptr && near(overlay->width(), window->width() / factor)
               && near(overlay->height(), window->height() / factor)
               && fillsWindow(overlay, window),
           prefix + QStringLiteral("Qt keeps the overlay at logical dimensions with a physical window-sized frame"),
           out, failed);
    if (overlay != nullptr && !fillsWindow(overlay, window)) {
        const QRectF scene = overlay->mapRectToScene(QRectF(0, 0, overlay->width(), overlay->height()));
        out << "  overlay logical " << overlay->width() << 'x' << overlay->height()
            << ", scale " << overlay->scale() << ", scene " << scene.x() << ',' << scene.y()
            << ' ' << scene.width() << 'x' << scene.height()
            << ", window " << window->width() << 'x' << window->height() << '\n';
        describeItem("overlay", overlay, out);
        describeItem("overlay parent", overlay->parentItem(), out);
        describeItem("host content item", itemProperty(window, "contentItem"), out);
        describeItem("window root item", window->contentItem(), out);
    }

    for (const char* name : {"scaleButton", "scaleText", "scaleIcon"}) {
        QQuickItem* item = window->findChild<QQuickItem*>(QString::fromLatin1(name));
        if (!expect(item != nullptr, prefix + QString::fromLatin1(name) + QStringLiteral(" exists"), out, failed)) {
            return;
        }
        const QRectF scene = item->mapRectToScene(QRectF(0, 0, item->width(), item->height()));
        expect(near(scene.width(), item->width() * factor)
                   && near(scene.height(), item->height() * factor),
               prefix + QString::fromLatin1(name) + QStringLiteral(" scales uniformly"), out, failed);
    }

    QQuickItem* button = window->findChild<QQuickItem*>(QStringLiteral("scaleButton"));
    const QPointF buttonCenter(button->width() / 2, button->height() / 2);
    const QPointF scenePoint = button->mapToScene(buttonCenter);
    const QPointF mappedBack = button->mapFromScene(scenePoint);
    const int clicks = window->property("clickCount").toInt();
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, scenePoint.toPoint());
    settle();
    expect(near(mappedBack.x(), buttonCenter.x()) && near(mappedBack.y(), buttonCenter.y())
               && window->property("clickCount").toInt() == clicks + 1,
           prefix + QStringLiteral("mouse input reaches the scaled button"), out, failed);

    QMetaObject::invokeMethod(window, "openDialog");
    settle();
    QQuickItem* dialog = itemProperty(window, "dialogVisual");
    if (!expect(dialog != nullptr, prefix + QStringLiteral("AppDialog has a visual popup item"), out, failed)) {
        QMetaObject::invokeMethod(window, "closeDialog");
        QTest::qWait(200);
        return;
    }
    const QRectF dialogScene = dialog->mapRectToScene(QRectF(0, 0, dialog->width(), dialog->height()));
    // Popup positions are rounded to whole layout pixels. An odd logical
    // window size can therefore move its center by half a scaled pixel.
    const qreal centerTolerance = factor / 2.0 + 0.01;
    expect(insideWindow(dialog, window)
               && std::abs(dialogScene.center().x() - window->width() / 2.0) <= centerTolerance
               && std::abs(dialogScene.center().y() - window->height() / 2.0) <= centerTolerance,
           prefix + QStringLiteral("AppDialog stays centered and inside the window"), out, failed);
    QQuickItem* dialogButton = window->findChild<QQuickItem*>(QStringLiteral("dialogButton"));
    if (!expect(dialogButton != nullptr, prefix + QStringLiteral("AppDialog contains its action button"), out, failed)) {
        QMetaObject::invokeMethod(window, "closeDialog");
        QTest::qWait(200);
        return;
    }
    const int dialogClicks = window->property("clickCount").toInt();
    const QPoint dialogPoint = dialogButton->mapToScene(
        QPointF(dialogButton->width() / 2, dialogButton->height() / 2)).toPoint();
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, dialogPoint);
    settle();
    expect(window->property("clickCount").toInt() == dialogClicks + 1,
           prefix + QStringLiteral("mouse input reaches the scaled overlay dialog"), out, failed);
    QMetaObject::invokeMethod(window, "closeDialog");
    QTest::qWait(200);

    QMetaObject::invokeMethod(window, "openCombo");
    settle();
    QQuickItem* combo = itemProperty(window, "comboVisual");
    expect(insideWindow(combo, window),
           prefix + QStringLiteral("the combo popup near the lower-right edge stays on screen"), out, failed);
    expect(fillsWindow(overlay, window),
           prefix + QStringLiteral("opening popups preserves the overlay frame"), out, failed);
    QMetaObject::invokeMethod(window, "closeCombo");
    QTest::qWait(200);
}

void expectSynchronizedScale(QObject* preferences, QQuickWindow* first, QQuickWindow* second,
                             int percent, const QString& message, QTextStream& out, int* failed)
{
    QQuickItem* firstSurface = first->findChild<QQuickItem*>(QStringLiteral("scaleSurface"));
    QQuickItem* secondSurface = second->findChild<QQuickItem*>(QStringLiteral("scaleSurface"));
    const qreal factor = percent / 100.0;
    expect(preferences->property("uiScalePercent").toInt() == percent
               && firstSurface != nullptr && secondSurface != nullptr
               && near(firstSurface->scale(), factor) && near(secondSurface->scale(), factor)
               && fillsWindow(firstSurface, first) && fillsWindow(secondSurface, second),
           message, out, failed);
}

void pressScaleKey(QQuickWindow* window, Qt::Key key, QTextStream& out, int* failed)
{
    window->requestActivate();
    expect(QTest::qWaitForWindowActive(window),
           window->objectName() + QStringLiteral(" receives keyboard focus"), out, failed);
#ifdef Q_OS_MACOS
    QTest::keyClick(window, key, Qt::MetaModifier | Qt::AltModifier);
#else
    QTest::keyClick(window, key, Qt::ControlModifier | Qt::AltModifier);
#endif
    settle();
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
    const std::unique_ptr<QObject> harness = createHarness(engine, out);
    if (!expect(harness != nullptr, QStringLiteral("the two real UI scale surfaces instantiate"), out, &failed)) {
        return 1;
    }
    QObject* preferences = harness->property("preferences").value<QObject*>();
    auto* first = harness->findChild<QQuickWindow*>(QStringLiteral("firstWindow"));
    auto* second = harness->findChild<QQuickWindow*>(QStringLiteral("secondWindow"));
    if (!expect(preferences != nullptr && first != nullptr && second != nullptr,
                QStringLiteral("both windows share one preferences object"), out, &failed)) {
        return 1;
    }
    expect(QTest::qWaitForWindowExposed(first) && QTest::qWaitForWindowExposed(second),
           QStringLiteral("both standalone windows expose their scenes"), out, &failed);
    settle();
    prepareControlIcons(first);
    prepareControlIcons(second);
    settle();
    for (int percent : {75, 100, 125, 200}) {
        preferences->setProperty("uiScalePercent", percent);
        settle();
        checkWindow(first, percent, out, &failed);
        checkWindow(second, percent, out, &failed);
    }
    first->resize(731, 527);
    second->resize(533, 419);
    settle();
    checkWindow(first, 200, out, &failed);
    checkWindow(second, 200, out, &failed);
    preferences->setProperty("uiScalePercent", 100);
    settle();
    checkWindow(first, 100, out, &failed);
    checkWindow(second, 100, out, &failed);

    pressScaleKey(first, Qt::Key_Equal, out, &failed);
    expectSynchronizedScale(preferences, first, second, 110,
                            QStringLiteral("Ctrl+Alt+= in the first window enlarges both windows"), out, &failed);
    pressScaleKey(second, Qt::Key_Minus, out, &failed);
    expectSynchronizedScale(preferences, first, second, 100,
                            QStringLiteral("Ctrl+Alt+- in the second window shrinks both windows"), out, &failed);
    preferences->setProperty("uiScalePercent", 150);
    settle();
    pressScaleKey(first, Qt::Key_0, out, &failed);
    expectSynchronizedScale(preferences, first, second, 100,
                            QStringLiteral("Ctrl+Alt+0 restores both windows to 100%"), out, &failed);
    preferences->setProperty("uiScalePercent", 200);
    settle();
    pressScaleKey(first, Qt::Key_Equal, out, &failed);
    expectSynchronizedScale(preferences, first, second, 200,
                            QStringLiteral("the enlargement shortcut stops at 200%"), out, &failed);
    preferences->setProperty("uiScalePercent", 75);
    settle();
    pressScaleKey(second, Qt::Key_Minus, out, &failed);
    expectSynchronizedScale(preferences, first, second, 75,
                            QStringLiteral("the shrink shortcut stops at 75%"), out, &failed);

    QObject* shortcuts = harness->property("shortcuts").value<QObject*>();
    shortcuts->setProperty("captureActive", true);
    preferences->setProperty("uiScalePercent", 135);
    settle();
    for (Qt::Key key : {Qt::Key_Equal, Qt::Key_Minus, Qt::Key_0}) {
        pressScaleKey(first, key, out, &failed);
        expectSynchronizedScale(preferences, first, second, 135,
                                QStringLiteral("shortcut capture suppresses scale command %1").arg(key), out, &failed);
    }
    shortcuts->setProperty("captureActive", false);
    out << (failed == 0 ? "QmlUiScale spec passed.\n" : "QmlUiScale spec failed: ");
    if (failed != 0) out << failed << '\n';
    return failed == 0 ? 0 : 1;
}
