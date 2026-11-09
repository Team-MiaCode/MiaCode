#include "preview/quick_scene/PreviewQuickSceneRoot.h"
#include "preview/runtime/PreviewRuntime.h"
#include "preview/runtime/PreviewSceneAssetRepository.h"
#include "core/scene/PreviewFireworkWarmupPolicy.h"

#include <QGuiApplication>
#include <QDir>
#include <QPainter>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTextStream>

#include <algorithm>

#ifndef MIACODE_TEST_OUTPUT_ROOT
#error "MIACODE_TEST_OUTPUT_ROOT must be an absolute workspace build directory"
#endif

namespace {
using namespace miacode::preview;

quint64 location(int line, int column)
{
    return (quint64(quint32(line)) << 32) | quint32(column);
}

bool writeSyntheticSkin(const QString& directory)
{
    QImage image(48, 48, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);
    painter.drawEllipse(QRectF(4, 4, 40, 40));
    painter.end();
    for (const QString& name : {QStringLiteral("tap.png"), QStringLiteral("hold.png"), QStringLiteral("star.png")})
        if (!image.save(QDir(directory).filePath(name))) return false;
    return true;
}

struct SelectionRequest {
    QSet<quint64> locations;
    bool additive = false;
};

bool verifyWindowInteraction(QTextStream& out, QTextStream& err)
{
    bool ok = true;
    const auto require = [&](bool condition, const char* label) {
        if (!condition) { ok = false; err << "FAIL: " << label << Qt::endl; }
        return condition;
    };
    QTemporaryDir skin(QDir(QStringLiteral(MIACODE_TEST_OUTPUT_ROOT))
        .filePath(QStringLiteral("preview-note-interaction-XXXXXX")));
    if (!require(skin.isValid() && writeSyntheticSkin(skin.path()), "create a synthetic skin inside the workspace build directory")) return false;

    PreviewRuntime runtime;
    auto* assets = runtime.findChild<miacode::preview::runtime::PreviewSceneAssetRepository*>();
    if (!require(assets && assets->loadSkinDirectorySync(skin.path()), "load the real runtime's synthetic core assets synchronously")) return false;

    QVector<TimelineNoteMarker> notes;
    for (int lane : {1, 5, 3}) {
        TimelineNoteMarker marker;
        marker.type = QStringLiteral("tap");
        marker.lane = lane;
        marker.second = lane == 3 ? 20.0 : 2.0;
        marker.sourceLine = 1;
        marker.sourceCol = lane == 1 ? 1 : lane == 5 ? 3 : 5;
        notes.append(marker);
    }
    runtime.setNoteMarkers(notes);
    runtime.setPlayheadSeconds(1.9);

    QQuickWindow window;
    window.resize(660, 640);
    // A nonzero scene offset catches accidental use of window coordinates as
    // item coordinates in the native event path.
    PreviewQuickSceneRoot root(window.contentItem());
    root.setPosition({30, 20});
    root.setSize({600, 600});
    root.setRuntime(&runtime);
    window.show();
    if (!require(QTest::qWaitForWindowExposed(&window, 3000), "expose a real QQuickWindow for Qt's pointer delivery")) return false;

    // Presentation can complete the production firework warm-up and bump the
    // scene revision once. Let that independent startup work settle before a
    // gesture freezes its geometry/revision snapshot.
    const auto warming = [&] {
        const auto snapshot = runtime.frameStateSnapshot();
        return std::any_of(snapshot->noteMarkers.cbegin(), snapshot->noteMarkers.cend(),
            [](const auto& marker) { return scene::isFireworkWarmupMarker(marker); });
    };
    for (int elapsed = 0; elapsed < 3000 && warming(); elapsed += 10) QTest::qWait(10);
    QCoreApplication::processEvents();

    const auto shapes = selection::visibleNoteHitShapes(*runtime.frameStateSnapshot(), {600, 600});
    const auto shapeFor = [&](quint64 id) -> QPainterPath {
        for (const auto& hit : shapes) if (hit.location == id) return hit.shape;
        return {};
    };
    const auto firstShape = shapeFor(location(1, 1));
    const auto secondShape = shapeFor(location(1, 3));
    if (!require(!firstShape.isEmpty() && !secondShape.isEmpty() && shapeFor(location(1, 5)).isEmpty(),
        "the fixture exposes two independent notes and excludes the future note")) return false;
    const auto interior = [](const QPainterPath& path) {
        const QRect bounds = path.boundingRect().toAlignedRect();
        for (int y = bounds.top(); y <= bounds.bottom(); ++y)
            for (int x = bounds.left(); x <= bounds.right(); ++x)
                if (path.contains(QPointF(x, y))) return QPoint(x, y);
        return QPoint(-1, -1);
    };
    const QPoint firstPoint = interior(firstShape);
    const QPoint secondPoint = interior(secondShape);
    const QRect firstBox = firstShape.boundingRect().adjusted(-4, -4, 4, 4).toAlignedRect();
    const auto inWindow = [&](const QPoint& point) { return root.mapToScene(point).toPoint(); };

    QSignalSpy starts(&runtime, &PreviewRuntime::noteSelectionStarted);
    QSignalSpy finishes(&runtime, &PreviewRuntime::noteSelectionFinished);
    QVector<SelectionRequest> requests;
    bool playing = false;
    int pauseRequests = 0;
    // PreviewRuntime publishes the pause request to its host. No audio backend,
    // document session or second playback clock is constructed in this spec.
    QObject::connect(&runtime, &PreviewRuntime::noteSelectionStarted, &window, [&] {
        ++pauseRequests;
        playing = false;
    });
    QObject::connect(&runtime, &PreviewRuntime::noteSelectionFinished, &window,
        [&](const QSet<quint64>& locations, bool additive) { requests.append({locations, additive}); });

    const auto press = [&](const QPoint& point, Qt::KeyboardModifiers modifiers) {
        QTest::mousePress(&window, Qt::LeftButton, modifiers, inWindow(point), 1);
    };
    const auto release = [&](const QPoint& point, Qt::KeyboardModifiers modifiers) {
        QTest::mouseRelease(&window, Qt::LeftButton, modifiers, inWindow(point), 1);
        QCoreApplication::processEvents();
    };
    const auto drag = [&](const QPoint& from, const QPoint& to, Qt::KeyboardModifiers modifiers) {
        press(from, modifiers);
        QTest::mouseMove(&window, inWindow(to), 1);
        release(to, modifiers);
    };
    const auto lastIs = [&](const QSet<quint64>& locations, bool additive) {
        return !requests.isEmpty() && requests.last().locations == locations && requests.last().additive == additive;
    };

    playing = true;
    press(firstBox.topLeft(), Qt::ShiftModifier);
    require(starts.count() == 1 && pauseRequests == 1 && !playing && finishes.isEmpty(),
        "Shift press requests pause immediately, before completing a selection");
    require(root.keepMouseGrab() && window.activeFocusItem() == &root, "Shift press owns the mouse grab and Escape focus");
    QTest::mouseMove(&window, inWindow(firstBox.bottomRight()), 1);
    release(firstBox.bottomRight(), Qt::ShiftModifier);
    require(finishes.count() == 1 && lastIs({location(1, 1)}, true) && !root.keepMouseGrab(),
        "Shift drag through the real window emits exactly the intersecting note with the toggle contract");

    drag(firstBox.bottomRight(), firstBox.topLeft(), Qt::ShiftModifier);
    require(finishes.count() == 2 && lastIs({location(1, 1)}, true),
        "reversing both drag axes selects the same geometry");

    press(secondPoint, Qt::ControlModifier);
    release(secondPoint, Qt::ControlModifier);
    require(finishes.count() == 2, "Ctrl click does not select a note");
    press(firstPoint, Qt::MetaModifier);
    release(firstPoint, Qt::MetaModifier);
    require(finishes.count() == 2, "Meta click does not select a note");

    drag({1, 1}, {598, 598}, Qt::ControlModifier | Qt::ShiftModifier);
    require(finishes.count() == 3 && lastIs({location(1, 1), location(1, 3)}, true),
        "Ctrl+Shift drag requests a union of visible geometry and excludes future notes");

    const int beforeCancel = finishes.count();
    press(firstBox.topLeft(), Qt::ShiftModifier);
    QTest::mouseMove(&window, inWindow(firstBox.bottomRight()), 1);
    QTest::keyClick(&window, Qt::Key_Escape);
    require(!root.keepMouseGrab(), "Escape releases the native pointer grab during a drag");
    release(firstBox.bottomRight(), Qt::ShiftModifier);
    require(finishes.count() == beforeCancel, "Escape cancels the pending gesture without publishing a replacement selection");

    press(firstBox.topLeft(), Qt::ShiftModifier);
    root.ungrabMouse();
    release(firstBox.bottomRight(), Qt::ShiftModifier);
    require(finishes.count() == beforeCancel, "native pointer ungrab cancels the pending gesture");

    press(firstBox.topLeft(), Qt::ShiftModifier);
    const auto oldRevision = runtime.frameStateSnapshot()->sceneContentRevision;
    auto edited = notes;
    edited[0].sourceLine = 9;
    runtime.setNoteMarkers(edited);
    require(runtime.frameStateSnapshot()->sceneContentRevision != oldRevision, "the chart edit installs a new scene revision");
    release(firstBox.bottomRight(), Qt::ShiftModifier);
    require(finishes.count() == beforeCancel, "release after a scene revision change discards the captured geometry");
    runtime.setNoteMarkers(notes);
    QCoreApplication::processEvents();

    const auto captured = runtime.frameStateSnapshot();
    press(firstBox.topLeft(), Qt::ShiftModifier);
    runtime.setPlayheadSeconds(3.0);
    require(runtime.frameStateSnapshot()->sceneContentRevision == captured->sceneContentRevision,
        "seeking changes the displayed time without replacing chart geometry storage");
    release(firstBox.bottomRight(), Qt::ShiftModifier);
    require(finishes.count() == beforeCancel, "release after a paused seek discards old hit shapes");
    runtime.setPlayheadSeconds(captured->playheadSeconds);

    press(firstBox.topLeft(), Qt::ShiftModifier);
    runtime.setPlayheadSeconds(captured->playheadSeconds + 0.05, false);
    release(firstBox.bottomRight(), Qt::ShiftModifier);
    require(finishes.count() == beforeCancel, "an advancing playback frame invalidates the drag snapshot");
    runtime.setPlayheadSeconds(captured->playheadSeconds);

    press(firstBox.topLeft(), Qt::ShiftModifier);
    runtime.setLayoutSquareScale(0.70);
    require(runtime.frameStateSnapshot()->render.layoutSquareScale != captured->render.layoutSquareScale,
        "the scale fixture changes the playfield geometry");
    release(firstBox.bottomRight(), Qt::ShiftModifier);
    require(finishes.count() == beforeCancel, "release after a playfield scale change discards old hit shapes");
    runtime.setLayoutSquareScale(captured->render.layoutSquareScale);

    press(firstBox.topLeft(), Qt::ShiftModifier);
    runtime.noteSelectionInvalidated();
    require(!root.keepMouseGrab(), "host document-revision invalidation cancels the drag before parser completion");
    release(firstBox.bottomRight(), Qt::ShiftModifier);
    require(finishes.count() == beforeCancel, "a host-invalidated gesture never publishes stale locations");

    drag({2, 2}, {20, 20}, Qt::ShiftModifier);
    require(finishes.count() == beforeCancel + 1 && lastIs({}, true), "a Shift rectangle over empty space has no hit to toggle");
    const int beforePlain = finishes.count(), startsBeforePlain = starts.count();
    press(firstPoint, Qt::NoModifier);
    release(firstPoint, Qt::NoModifier);
    require(finishes.count() == beforePlain && starts.count() == startsBeforePlain,
        "an ordinary left click does not enter the selection gesture or request pause");

    press(firstBox.topLeft(), Qt::ShiftModifier);
    const auto oldStorage = runtime.frameStateSnapshot()->sceneStorageGeneration;
    runtime.reset(true);
    require(runtime.frameStateSnapshot()->sceneStorageGeneration != oldStorage, "document replacement releases the old scene storage generation");
    release(firstBox.bottomRight(), Qt::ShiftModifier);
    require(finishes.count() == beforePlain, "document replacement discards a gesture captured against old marker storage");
    require(starts.isValid() && finishes.isValid(), "Qt observes the real runtime selection signals");

    out << "preview_note_selection_interaction_spec " << (ok ? "PASS" : "FAIL")
        << " pause_requests=" << pauseRequests << " completed_requests=" << finishes.count() << Qt::endl;
    window.hide();
    return ok;
}
}

int main(int argc, char** argv)
{
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    QGuiApplication app(argc, argv);
    qRegisterMetaType<QSet<quint64>>();
    QTextStream out(stdout), err(stderr);
    return verifyWindowInteraction(out, err) ? 0 : 1;
}
