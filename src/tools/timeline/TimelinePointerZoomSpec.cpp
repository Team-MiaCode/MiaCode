#include "timeline/TimelineSceneStateBuilder.h"
#include "timeline/quick/TimelineQuickItem.h"
#include "timeline/quick/TimelineQuickStateBridge.h"

#include <QGuiApplication>
#include <QTextStream>
#include <QWheelEvent>
#include <QtMath>

namespace {

class WheelTestTimeline : public TimelineQuickItem
{
public:
    using TimelineQuickItem::wheelEvent;
};

double secondAt(const TimelineQuickStateBridge& bridge, qreal x)
{
    using miacode::timeline::TimelineSceneBuildRequest;
    using miacode::timeline::TimelineSceneStateBuilder;
    TimelineSceneBuildRequest request;
    request.snapshot = bridge.renderSnapshot();
    request.viewportSize = bridge.viewportSize();
    request.zoomScale = bridge.zoomScale();
    request.fitViewportHeight = true;
    const auto layout = TimelineSceneStateBuilder::layoutMetrics(request);
    const qreal zeroX = TimelineSceneStateBuilder::secondToSceneXExact(layout, 0.0);
    return (x + bridge.horizontalScrollValue() - zeroX) / layout.pixelsPerSecond;
}

bool closeTo(double left, double right)
{
    return qAbs(left - right) < 1e-6;
}

}  // namespace

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    QTextStream err(stderr);
    TimelineQuickStateBridge bridge;
    TimelineRenderSnapshot snapshot;
    snapshot.durationSeconds = 180.0;
    bridge.setTimelineData(snapshot);

    WheelTestTimeline timeline;
    timeline.setSize({1001, 360});
    timeline.setStateBridge(&bridge);
    bridge.setHorizontalScrollValue(1500.25);

    const QPointF mouse(713.75, 120);
    const auto wheel = [&](Qt::KeyboardModifiers modifiers, int angle, int pixel = 0) {
        QWheelEvent event(mouse, mouse, QPoint(0, pixel), QPoint(0, angle), Qt::NoButton,
                          modifiers, Qt::NoScrollPhase, false);
        event.setAccepted(false);
        timeline.wheelEvent(&event);
        return event.isAccepted();
    };

    const double anchoredSecond = secondAt(bridge, mouse.x());
    if (!wheel(Qt::ControlModifier, 120)
        || !closeTo(bridge.zoomScale(), 0.6)
        || !closeTo(secondAt(bridge, mouse.x()), anchoredSecond)
        || !wheel(Qt::ControlModifier, 0, -30)
        || !closeTo(bridge.zoomScale(), 0.6 / qSqrt(1.2))
        || !closeTo(secondAt(bridge, mouse.x()), anchoredSecond)) {
        err << "The configured wheel must preserve the exact mouse anchor on angle and pixel input\n";
        return 1;
    }

    const double currentScale = bridge.zoomScale();
    if (!wheel(Qt::AltModifier, 120) || !closeTo(bridge.zoomScale(), currentScale)) {
        err << "An unconfigured Alt-wheel gesture must not zoom\n";
        return 1;
    }
    bridge.setZoomWheelShortcuts({QStringLiteral("Shift+WheelDown")},
                                 {QStringLiteral("Shift+WheelUp")});
    if (!wheel(Qt::ControlModifier, 120) || !closeTo(bridge.zoomScale(), currentScale)
        || !wheel(Qt::AltModifier, -120) || !closeTo(bridge.zoomScale(), currentScale)) {
        err << "After rebinding, unconfigured Ctrl and Alt gestures must not zoom\n";
        return 1;
    }
    if (!wheel(Qt::ShiftModifier, -120) || !closeTo(bridge.zoomScale(), currentScale * 1.2)
        || !wheel(Qt::ShiftModifier, 120) || !closeTo(bridge.zoomScale(), currentScale)) {
        err << "Custom wheel direction bindings must remain authoritative\n";
        return 1;
    }
    const double centerBefore = bridge.viewportCenterSecond();
    bridge.stepZoomPresets(2);
    if (!closeTo(bridge.zoomScale(), 1.0)
        || !closeTo(bridge.viewportCenterSecond(), centerBefore)) {
        err << "The zoom control must step from an intermediate scale to centered presets\n";
        return 1;
    }
    bridge.stepZoomOut();
    if (!closeTo(bridge.zoomScale(), 0.75)) {
        err << "Keyboard zoom commands must still step through presets\n";
        return 1;
    }
    bridge.applyZoomPreset(1.5);
    if (!closeTo(bridge.zoomScale(), 1.5)) {
        err << "The preset menu must keep its discrete values\n";
        return 1;
    }
    bridge.setZoomWheelShortcuts({}, {});
    if (!wheel(Qt::ControlModifier, 120 * 20)
        || !closeTo(bridge.zoomScale(), 3.0)
        || !wheel(Qt::ControlModifier, -120 * 40)
        || !closeTo(bridge.zoomScale(), 0.25)) {
        err << "Continuous zoom must stay within the supported range\n";
        return 1;
    }

    QTextStream(stdout) << "timeline_pointer_zoom_spec ok\n";
    return 0;
}
