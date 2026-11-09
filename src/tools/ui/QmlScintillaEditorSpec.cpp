#include "scintillaquick_validation_access.h"
#include "app/ui/editor/ScintillaDocumentAdapter.h"
#include "app/ui/editor/ScintillaDslStyler.h"
#include "app/services/EditorTypography.h"
#include "app/ui/editor/EditorContextMenu.h"

#include <QGuiApplication>
#include <QEventLoop>
#include <QImage>
#include <QFile>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QTextStream>
#include <QTimer>
#include <cstring>
#include <cmath>
#include <memory>

namespace {
using Scintilla::Internal::ScintillaQuick_validation_access;

bool checkFrame(ScintillaQuick_item& editor, const QString& expected, QTextStream& out)
{
    const auto frame = ScintillaQuick_validation_access::capture_cached_frame(editor);
    QString captured;
    for (const auto& line : frame.visual_lines)
        for (const auto& run : line.text_runs) captured += run.text;
    out << "native=" << editor.property("text").toString()
        << " captured=" << captured << " textRect=" << frame.text_rect.width()
        << 'x' << frame.text_rect.height() << " height=" << editor.send(SCI_TEXTHEIGHT, 0)
        << " readonly=" << editor.send(SCI_GETREADONLY) << Qt::endl;
    return editor.property("text").toString() == expected && captured.contains(expected)
        && frame.text_rect.width() > 0 && frame.text_rect.height() > 0;
}

bool testUndoSelection(QTextStream& out)
{
    ScintillaQuick_item editor;
    miacode::ui::ScintillaDocumentAdapter document(editor);
    QObject::connect(&editor, &ScintillaQuick_item::notificationReceived, &editor,
        [&](const ScintillaQuick_notification& notification) {
            const int flags = int(notification.modificationType);
            if (notification.code != Scintilla::Notification::Modified
                || !(flags & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT))) return;
            document.applyChange(notification.position,
                flags & SC_MOD_DELETETEXT ? QString::fromUtf8(notification.text).size() : 0,
                flags & SC_MOD_INSERTTEXT ? notification.text : QByteArray{});
        });
    const QString original = QStringLiteral("中文😀(120){4}1,2,3,4,");
    const int start = original.indexOf(QStringLiteral("1,2,"));
    const int end = start + 4;
    const auto selectionMatches = [&](int anchor, int caret) {
        return editor.send(SCI_GETANCHOR) == document.bytePosition(anchor)
            && editor.send(SCI_GETCURRENTPOS) == document.bytePosition(caret);
    };
    document.activate(QStringLiteral("difficulty:5"), original);
    editor.send(SCI_SETSELECTION, document.bytePosition(end), document.bytePosition(start));
    editor.send(SCI_CLEAR);
    const QString deleted = document.text();
    document.activate(QStringLiteral("difficulty:6"), QStringLiteral("7,8,"));
    document.activate(QStringLiteral("difficulty:5"), deleted);
    document.undo();
    if (document.text() != original || !selectionMatches(start, end)) {
        out << "undo deletion did not restore text and its selection after switching difficulties" << Qt::endl;
        return false;
    }
    editor.send(SCI_SETSELECTION, document.bytePosition(start), document.bytePosition(end));
    editor.sends(SCI_REPLACESEL, 0, "A1,");
    document.undo();
    if (document.text() != original || !selectionMatches(end, start)) {
        out << "undo replacement did not restore the reversed selection" << Qt::endl;
        return false;
    }
    editor.send(SCI_REDO);
    if (document.text() != QString(original).replace(start, end - start, QStringLiteral("A1,"))) {
        out << "selection restoration broke redo" << Qt::endl;
        return false;
    }
    document.undo();
    editor.send(SCI_SETSELECTION, document.bytePosition(end), document.bytePosition(start));
    editor.send(SCI_BEGINUNDOACTION);
    editor.send(SCI_SETTARGETSTART, document.bytePosition(start));
    editor.send(SCI_SETTARGETEND, document.bytePosition(end));
    editor.sends(SCI_REPLACETARGET, 6, "8,7,6,");
    editor.send(SCI_ADDUNDOACTION, 42, 0);
    editor.send(SCI_ENDUNDOACTION);
    editor.send(SCI_SETSELECTION, document.bytePosition(start + 6), document.bytePosition(start));
    document.undo();
    if (document.text() != original || !selectionMatches(start, end)) {
        out << "undo grouped range transform did not restore its original selection" << Qt::endl;
        return false;
    }
    editor.send(SCI_SETEMPTYSELECTION, document.bytePosition(end));
    QKeyEvent key(QEvent::KeyPress, Qt::Key_5, Qt::NoModifier, QStringLiteral("5"));
    QCoreApplication::sendEvent(&editor, &key);
    document.undo();
    if (document.text() != original || !selectionMatches(end, end)) {
        out << "plain typing undo changed the normal caret behavior" << Qt::endl;
        return false;
    }
    out << "undo selection checks passed" << Qt::endl;
    return true;
}

bool testEditor(QTextStream& out, const QString& capturePath)
{
    QQuickWindow window;
    window.resize(800, 600);
    ScintillaQuick_item editor(window.contentItem());
    editor.setPosition({40, 50});
    editor.setSize({700, 500});
    miacode::ui::ScintillaDocumentAdapter document(editor);
    miacode::ui::ScintillaDslStyler styler(editor, document);
    QObject::connect(&editor, &ScintillaQuick_item::notificationReceived, &editor,
        [&](const ScintillaQuick_notification& notification) {
            const int flags = int(notification.modificationType);
            if (notification.code != Scintilla::Notification::Modified
                || !(flags & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT))) return;
            document.applyChange(notification.position,
                flags & SC_MOD_DELETETEXT ? QString::fromUtf8(notification.text).size() : 0,
                flags & SC_MOD_INSERTTEXT ? notification.text : QByteArray{});
        });

    const QFont font(QStringLiteral("Consolas"), 11);
    editor.setProperty("font", font);
    const QVariantMap palette{
        {"text", QColor(Qt::white)}, {"background", QColor(32, 32, 32, 185)},
        {"lineNumber", QColor(Qt::gray)}, {"accent", QColor(Qt::cyan)},
        {"keyword", QColor(Qt::green)}, {"duration", QColor(Qt::yellow)},
        {"comment", QColor(Qt::gray)}, {"error", QColor(Qt::red)},
        {"warning", QColor(Qt::yellow)}, {"follow", QColor(Qt::blue)},
        {"currentLine", QColor(80, 80, 80, 40)}, {"selection", QColor(80, 80, 120, 60)},
        {"followOpacity", 0.2}};
    editor.setProperty("editorColors", palette);
    editor.setProperty("sceneBackgroundColor", palette.value("background"));
    styler.setAppearance(font, palette);
    const QString initial = QStringLiteral("(120){4}1,2,3,4,");
    document.activate(QStringLiteral("difficulty:5"), initial);
    styler.reset();
    styler.style();
    if (!checkFrame(editor, initial, out)) return false;

    const QByteArray syntaxSample("(180){4}1,2x-5>7^1[8:1],3b,4h[4:1],ABCDE,<HS*2>, ||b>");
    document.activate(QStringLiteral("difficulty:5"), QString::fromUtf8(syntaxSample));
    styler.reset();
    styler.style();
    for (char ch : QByteArray("x->^bhABCDE")) {
        if (editor.send(SCI_GETSTYLEAT, syntaxSample.indexOf(ch)) != 5) return false;
    }
    if (editor.send(SCI_GETSTYLEAT, syntaxSample.indexOf('[')) != 2
        || editor.send(SCI_GETSTYLEAT, syntaxSample.indexOf('H')) != 6
        || editor.send(SCI_GETSTYLEAT, syntaxSample.lastIndexOf('b')) != 3) return false;
    document.activate(QStringLiteral("difficulty:5"), initial);
    styler.reset();
    styler.style();

    editor.send(SCI_GOTOPOS, editor.send(SCI_GETLENGTH));
    QKeyEvent key(QEvent::KeyPress, Qt::Key_5, Qt::NoModifier, QStringLiteral("5"));
    QCoreApplication::sendEvent(&editor, &key);
    styler.invalidate(0, 0);
    styler.style();
    if (!checkFrame(editor, initial + QLatin1Char('5'), out)
        || document.text() != initial + QLatin1Char('5')) return false;

    document.activate(QStringLiteral("difficulty:6"), QStringLiteral("7,8,"));
    styler.reset();
    styler.style();
    if (!checkFrame(editor, QStringLiteral("7,8,"), out)) return false;
    document.activate(QStringLiteral("difficulty:5"), initial + QLatin1Char('5'));
    styler.reset();
    styler.style();
    if (!checkFrame(editor, initial + QLatin1Char('5'), out)) return false;
    styler.follow(true, 0, 7);
    editor.send(SCI_SETSELECTION, 10, 2);
    if (!checkFrame(editor, initial + QLatin1Char('5'), out)) return false;

    const auto anchorBefore = editor.send(SCI_GETANCHOR);
    const auto caretBefore = editor.send(SCI_GETCURRENTPOS);
    const auto undoBefore = editor.send(SCI_CANUNDO);
    for (const auto& point : {QPointF(700, 400), QPointF(130, 10)}) {
        miacode::ui::prepareEditorContextMenu(editor, point);
        if (editor.send(SCI_GETANCHOR) != anchorBefore || editor.send(SCI_GETCURRENTPOS) != caretBefore
            || editor.send(SCI_CANUNDO) != undoBefore) {
            out << "context menu lost the existing selection" << Qt::endl;
            return false;
        }
    }
    editor.send(SCI_SETEMPTYSELECTION, 0);
    const QPointF caretTarget(editor.send(SCI_POINTXFROMPOSITION, 0, 6),
                              editor.send(SCI_POINTYFROMPOSITION, 0, 6) + 1);
    miacode::ui::prepareEditorContextMenu(editor, caretTarget);
    if (editor.send(SCI_GETCURRENTPOS) != 6 || !editor.send(SCI_GETSELECTIONEMPTY)) {
        out << "context menu without selection did not move the caret" << Qt::endl;
        return false;
    }
    editor.send(SCI_SETSELECTION, caretBefore, anchorBefore);
    const QFont changedFont = miacode::editor_typography::resolveFont(
        QFontDatabase::systemFont(QFontDatabase::GeneralFont).family(), 17);
    editor.setProperty("font", changedFont);
    styler.setAppearance(changedFont, palette);
    if (editor.send(SCI_GETANCHOR) != anchorBefore || editor.send(SCI_GETCURRENTPOS) != caretBefore
        || editor.send(SCI_CANUNDO) != undoBefore || document.text() != initial + QLatin1Char('5')) {
        out << "font change lost editor state" << Qt::endl;
        return false;
    }

    QEventLoop loop;
    QObject::connect(&window, &QQuickWindow::frameSwapped, &loop, &QEventLoop::quit);
    QTimer::singleShot(2000, &loop, &QEventLoop::quit);
    window.show();
    loop.exec();
    styler.follow(false, 0, 0);
    editor.send(SCI_SETEMPTYSELECTION, 0);
    window.requestUpdate();
    QTimer::singleShot(2000, &loop, &QEventLoop::quit);
    loop.exec();
    styler.follow(true, 0, 7);
    editor.send(SCI_SETSELECTION, 10, 2);
    window.requestUpdate();
    QTimer::singleShot(2000, &loop, &QEventLoop::quit);
    loop.exec();
    const QImage image = window.grabWindow();
    if (!capturePath.isEmpty()) image.save(capturePath);
    int glyphPixels = 0;
    for (int y = 50; y < qMin(100, image.height()); ++y)
        for (int x = 100; x < qMin(500, image.width()); ++x) {
            const QColor pixel = image.pixelColor(x, y);
            if (pixel.green() > pixel.red() + 40 && pixel.green() > pixel.blue() + 40)
                ++glyphPixels;
        }
    out << "image=" << image.width() << 'x' << image.height()
        << " glyphPixels=" << glyphPixels << Qt::endl;
    return glyphPixels > 10;
}

bool testLineNumberMargin(QTextStream& out)
{
    QQuickWindow window;
    ScintillaQuick_item editor(window.contentItem());
    editor.setSize({700, 500});
    miacode::ui::ScintillaDocumentAdapter document(editor);
    miacode::ui::ScintillaDslStyler styler(editor, document);
    document.activate(QStringLiteral("margin-probe"), QStringLiteral("(120){4}1,\n\n|| comment\n2,"));
    styler.reset();
    styler.style();
    ScintillaQuick_validation_access::capture_cached_frame(editor);

    int clicks = 0;
    int clickedLine = -1;
    int clickedMargin = -1;
    QObject::connect(&editor, &ScintillaQuick_item::marginClicked, &editor,
        [&](Scintilla::Position position, Scintilla::KeyMod, int margin) {
            ++clicks;
            clickedLine = editor.send(SCI_LINEFROMPOSITION, position) + 1;
            clickedMargin = margin;
        });
    const auto click = [&](qreal x, int line) {
        const int position = editor.send(SCI_POSITIONFROMLINE, line - 1);
        const qreal y = editor.send(SCI_POINTYFROMPOSITION, 0, position)
            + editor.send(SCI_TEXTHEIGHT, line - 1) / 2.0;
        const QPointF point(x, y);
        QMouseEvent press(QEvent::MouseButtonPress, point, point,
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease, point, point,
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&editor, &press);
        QCoreApplication::sendEvent(&editor, &release);
    };
    const int numberWidth = editor.send(SCI_GETMARGINWIDTHN, 0);
    const int bookmarkWidth = editor.send(SCI_GETMARGINWIDTHN, 1);
    bool passed = numberWidth > 0 && bookmarkWidth > 0;
    for (int line : {1, 2, 3, 4}) {
        const int previousClicks = clicks;
        click(numberWidth / 2.0, line);
        passed &= clicks == previousClicks + 1 && clickedMargin == 0 && clickedLine == line;
    }
    const int previousClicks = clicks;
    click(numberWidth + bookmarkWidth / 2.0, 3);
    passed &= clicks == previousClicks + 1 && clickedMargin == 1 && clickedLine == 3;
    out << "line_number_margin=" << (passed ? "PASS" : "FAIL")
        << " clicks=" << clicks << " last_margin=" << clickedMargin
        << " last_line=" << clickedLine << Qt::endl;
    return passed;
}

bool testWindowLayout(QTextStream& out)
{
    QFile source(QStringLiteral(MIACODE_SOURCE_ROOT "/src/app/ui/editor/SourceEditor.qml"));
    if (!source.open(QIODevice::ReadOnly)) return false;
    const QString qml = QString::fromUtf8(source.readAll());
    const int begin = qml.indexOf(QStringLiteral("readonly property int editorRailBaseWidth:"));
    const int end = qml.indexOf(QStringLiteral("readonly property int editorRailDisplayLineCount:"), begin);
    if (begin < 0 || end < begin) return false;
    const int marginBegin = qml.indexOf(QStringLiteral("readonly property real editorRailRightMargin:"));
    const int marginEnd = qml.indexOf(QStringLiteral("signal normalizeChartRequested()"), marginBegin);
    const int alignBegin = qml.indexOf(QStringLiteral("function alignRailCoordinate("));
    const int alignEnd = qml.indexOf(QStringLiteral("function overviewMarkerTop("), alignBegin);
    if (marginBegin < 0 || marginEnd < marginBegin || alignBegin < 0 || alignEnd < alignBegin) return false;

    qmlRegisterType<ScintillaQuick_item>("EditorProbe", 1, 0, "Editor");
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(MIACODE_QML_SPEC_IMPORT_ROOT));
    QQmlComponent component(&engine);
    component.setData((QStringLiteral(R"(
import QtQuick
import QtQuick.Controls
import EditorProbe 1.0
import MiaCode.UI
Item {
    id: root
    width: 700
    height: 500
)") + qml.mid(begin, end - begin)
        + QStringLiteral("readonly property real editorRailSceneX: root.mapToItem(null, 0, 0).x\n")
        + qml.mid(marginBegin, marginEnd - marginBegin)
        + qml.mid(alignBegin, alignEnd - alignBegin) + QStringLiteral(R"(
    Editor {
        objectName: "editor"
        anchors.left: parent.left
        anchors.right: rail.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
    }
    ScrollBar {
        id: rail
        objectName: "rail"
        anchors.right: parent.right
        anchors.rightMargin: root.editorRailRightMargin
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        orientation: Qt.Vertical
        width: root.editorRailWidth
    }
}
)" )).toUtf8(), QUrl());
    std::unique_ptr<QObject> object(component.create());
    if (!object) { out << component.errorString() << Qt::endl; return false; }
    QQuickWindow window;
    auto* root = qobject_cast<QQuickItem*>(object.get());
    root->setParentItem(window.contentItem());
    QCoreApplication::processEvents();
    auto* editor = root->findChild<QQuickItem*>(QStringLiteral("editor"));
    auto* rail = root->findChild<QQuickItem*>(QStringLiteral("rail"));
    out << "mounted_dpr=" << root->property("editorRailDevicePixelRatio").toDouble()
        << " editor_width=" << editor->width() << " rail_width=" << rail->width() << Qt::endl;
    const bool passed = std::isfinite(root->property("editorRailDevicePixelRatio").toDouble())
        && rail->width() > 0 && editor->width() > 600;
    root->setParentItem(nullptr);
    return passed;
}
}

int main(int argc, char** argv)
{
    QQuickWindow::setGraphicsApi(argc > 2 && std::strcmp(argv[2], "d3d11") == 0
        ? QSGRendererInterface::Direct3D11 : QSGRendererInterface::Software);
    QGuiApplication app(argc, argv);
    QTextStream out(stdout);
    const bool passed = testUndoSelection(out)
        && testEditor(out, argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString{})
        && testLineNumberMargin(out) && testWindowLayout(out);
    out << (passed ? "PASS" : "FAIL") << Qt::endl;
    return passed ? 0 : 1;
}
