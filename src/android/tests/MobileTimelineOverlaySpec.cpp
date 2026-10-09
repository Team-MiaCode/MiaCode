#include "timeline/quick/TimelineQuickOverlayLayer.h"

#include <QDir>
#include <QImage>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QtTest>

namespace {
class OverlayItem : public QQuickItem {
public:
    explicit OverlayItem(QQuickItem* parent) : QQuickItem(parent) {
        setFlag(ItemHasContents);
        setSize(QSizeF(160, 160));
        state.viewportSize = QSize(160, 160);
        state.timelineLeft = 20;
        state.timelineTop = 30;
        state.timelineHeight = 100;
        state.headerMarkerRightLimit = 160;
        state.playheadLine = {{80, 0}, {80, 160}, QColor(255, 20, 20), 2};
    }
    miacode::timeline::TimelineSceneState state;
    void refresh() { ++state.overlayDynamicRevision; update(); }
protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override {
        return layer.updateNode(oldNode, state, window(), nullptr);
    }
private:
    TimelineQuickOverlayLayer layer;
};

bool redNear(const QImage& image, const QPointF& logicalPoint) {
    const QPoint point = (logicalPoint * image.devicePixelRatio()).toPoint();
    for (int y = point.y() - 2; y <= point.y() + 2; ++y) {
        for (int x = point.x() - 2; x <= point.x() + 2; ++x) {
            if (image.rect().contains(x, y)) {
                const QColor color = image.pixelColor(x, y);
                // A thin line at fractional scale has partial pixel coverage.
                if (color.red() > 100 && color.green() < color.red() / 2 && color.blue() < color.red() / 2) return true;
            }
        }
    }
    return false;
}
}

class MobileTimelineOverlaySpec : public QObject {
    Q_OBJECT
private slots:
    void visibleLineStaysInsideClip_data() {
        QTest::addColumn<qreal>("rotation");
        QTest::addColumn<qreal>("scale");
        QTest::newRow("normal") << qreal(0) << qreal(1);
        QTest::newRow("fractional-scale") << qreal(0) << qreal(0.73);
        QTest::newRow("stencil-clip") << qreal(17) << qreal(0.73);
    }
    void visibleLineStaysInsideClip() {
        QFETCH(qreal, rotation);
        QFETCH(qreal, scale);
        QQuickWindow window;
        window.setColor(Qt::black);
        window.resize(300, 300);
        OverlayItem item(window.contentItem());
        item.setPosition({70, 70});
        item.setTransformOrigin(QQuickItem::Center);
        item.setScale(scale);
        item.setRotation(rotation);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTest::qWait(80);
        auto capture = [&](const QString& stage) {
            window.requestUpdate();
            QTest::qWait(80);
            QImage image = window.grabWindow();
            const QString output = qEnvironmentVariable("MIACODE_TIMELINE_RENDER_OUTPUT");
            if (!output.isEmpty() && !image.isNull()) {
                QDir().mkpath(output);
                image.save(QDir(output).filePath(QString::fromLatin1(QTest::currentDataTag()) + '-' + stage + ".png"));
            }
            return image;
        };
        const auto initial = capture("initial");
        QVERIFY(!initial.isNull());
        const auto graphicsApi = window.rendererInterface()->graphicsApi();
        qInfo() << "Graphics API:" << graphicsApi;
        if (qEnvironmentVariableIntValue("MIACODE_TIMELINE_REQUIRE_RHI")) {
            QVERIFY(graphicsApi != QSGRendererInterface::Software && graphicsApi != QSGRendererInterface::Unknown);
        }
        QVERIFY(!redNear(initial, item.mapToScene({80, 80})));

        item.state.hasPlayheadLine = true;
        item.refresh();
        const auto visible = capture("visible");
        QVERIFY(!visible.isNull());
        QVERIFY2(redNear(visible, item.mapToScene({80, 80})), "playhead must appear after an initially empty frame");
        QVERIFY2(!redNear(visible, item.mapToScene({80, 15})), "playhead must not escape above timeline clip");
        QVERIFY2(!redNear(visible, item.mapToScene({80, 145})), "playhead must not escape below timeline clip");

        item.state.horizontalScrollValue = 20;
        item.state.playheadLine.start.setX(100);
        item.state.playheadLine.end.setX(100);
        item.refresh();
        const auto scrolled = capture("scrolled");
        QVERIFY(redNear(scrolled, item.mapToScene({80, 80})));
        QVERIFY(!redNear(scrolled, item.mapToScene({100, 80})));

        item.state.timelineHeight = 50;
        ++item.state.layoutRevision;
        item.refresh();
        const auto resized = capture("resized");
        QVERIFY(redNear(resized, item.mapToScene({80, 50})));
        QVERIFY(!redNear(resized, item.mapToScene({80, 100})));

        item.state.hasPlayheadLine = false;
        item.refresh();
        QVERIFY(!redNear(capture("suppressed"), item.mapToScene({80, 80})));
    }
};

QTEST_MAIN(MobileTimelineOverlaySpec)
#include "MobileTimelineOverlaySpec.moc"
