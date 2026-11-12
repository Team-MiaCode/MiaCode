#include "timeline/quick/TimelineQuickItem.h"
#include "timeline/quick/TimelineQuickStateBridge.h"
#include "timeline/TimelineQuickModel.h"
#include "timeline/TimelineSceneStateBuilder.h"

#include <QGuiApplication>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSignalSpy>
#include <QTest>
#include <QTextStream>

int main(int argc, char** argv)
{
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    QGuiApplication app(argc, argv);
    qRegisterMetaType<QSet<quint64>>();
    bool ok = true;
    const auto expect = [&](bool pass, const char* label) {
        ok &= pass; QTextStream(pass ? stdout : stderr) << (pass ? "PASS " : "FAIL ") << label << '\n';
    };
    TimelineQuickModel model;
    model.rebuildFromText(QStringLiteral("(120){4}1,5,2h[4:1],6,E"), 0);
    QQuickWindow window;
    window.resize(1000, 400);
    TimelineQuickStateBridge bridge;
    TimelineQuickItem item(window.contentItem());
    item.setSize(QSizeF(1000,400));
    item.setStateBridge(&bridge);
    bridge.setFollowProgressEnabled(false);
    bridge.setTimelineData(model.snapshot());
    window.show(); window.requestActivate();
    QTest::qWait(100);
    miacode::timeline::TimelineSceneBuildRequest request;
    request.snapshot = bridge.renderSnapshot(); request.viewportSize = QSize(1000,400);
    request.zoomScale = bridge.zoomScale(); request.contentScale = bridge.contentScale();
    request.horizontalScrollValue = bridge.horizontalScrollValue(); request.fitViewportHeight = true;
    const auto state = miacode::timeline::TimelineSceneStateBuilder::build(request);
    expect(!state.noteHitAreas.isEmpty(), "synthetic timeline exposes native note hit geometry");
    if (state.noteHitAreas.isEmpty()) return 1;
    const auto hit = state.noteHitAreas.first();
    const QPoint center = (hit.rect.center() - QPointF(bridge.horizontalScrollValue(),0)).toPoint();
    const QPoint from = center - QPoint(7,7), to = center + QPoint(7,7);
    QSignalSpy selections(&bridge,&TimelineQuickStateBridge::noteSelectionRequested);
    const auto press = [&](const QPoint& p, Qt::KeyboardModifiers modifiers) { QTest::mousePress(&window,Qt::LeftButton,modifiers,p); };
    const auto release = [&](const QPoint& p) { QTest::mouseRelease(&window,Qt::LeftButton,Qt::ShiftModifier,p); QCoreApplication::processEvents(); };
    press(from,Qt::ShiftModifier); QTest::mouseMove(&window,to); release(to);
    expect(selections.size() == 1 && selections.last()[0].value<QSet<quint64>>().contains(hit.locationId)
        && selections.last()[1].toBool(), "Shift rectangle selects timeline notes with toggle semantics");
    press(to,Qt::ShiftModifier); QTest::mouseMove(&window,from); release(from);
    expect(selections.size() == 2 && selections.last()[0].value<QSet<quint64>>().contains(hit.locationId),
        "reversed rectangle uses the same geometry");
    press(from,Qt::ShiftModifier); QTest::keyClick(&window,Qt::Key_Escape); release(to);
    expect(selections.size() == 2 && !item.keepMouseGrab(), "Escape cancels timeline selection");
    press(from,Qt::ShiftModifier); item.ungrabMouse(); release(to);
    expect(selections.size() == 2, "lost pointer grab cancels selection");
    press(from,Qt::ShiftModifier); bridge.clear(); release(to);
    expect(selections.size() == 2, "document replacement discards captured note geometry");
    bridge.setTimelineData(model.snapshot());
    press(center,Qt::ControlModifier); QTest::mouseRelease(&window,Qt::LeftButton,Qt::ControlModifier,center);
    expect(selections.size() == 2, "Ctrl click never enters note selection");
    window.hide();
    return ok ? 0 : 1;
}
