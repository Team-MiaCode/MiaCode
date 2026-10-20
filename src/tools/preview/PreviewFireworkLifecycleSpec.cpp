#include <QCoreApplication>
#include <QImage>
#include <QRectF>
#include <QTextStream>
#include <cmath>

#include "core/scene/PreviewActiveMarkerView.h"
#include "core/scene/PreviewFrameState.h"
#include "core/scene/PreviewJudgeFireworkLayerState.h"
#include "core/video/PreviewGameplayConfig.h"

namespace {

using miacode::preview::scene::PreviewFrameState;
using miacode::preview::scene::PreviewJudgeFireworkLayerState;

constexpr qreal kEpsilon = 1e-3;

bool require(bool condition, const QString& message, QTextStream& err)
{
    if (!condition) {
        err << message << Qt::endl;
        return false;
    }
    return true;
}

bool requireNear(qreal actual, qreal expected, const QString& label, QTextStream& err)
{
    if (qAbs(actual - expected) <= kEpsilon) {
        return true;
    }
    err << label << " expected " << expected << " but got " << actual << Qt::endl;
    return false;
}

PreviewFrameState buildStateAt(qreal clipTimeSeconds)
{
    PreviewFrameState state;

    TimelineNoteMarker marker;
    marker.type = QStringLiteral("touch_hold");
    marker.isFirework = true;
    marker.endSecond = 0.0;
    marker.touchPoint = QPointF(270.0, 270.0);
    state.noteMarkers.append(marker);
    state.skin.touchPointImage = QImage(32, 32, QImage::Format_ARGB32_Premultiplied);
    state.skin.touchPointImage.fill(Qt::white);

    QImage colorBallImage(32, 32, QImage::Format_ARGB32_Premultiplied);
    colorBallImage.fill(qRgba(255, 255, 255, 255));
    state.judgeEffect.fireworkColorBallImage = colorBallImage;
    state.judgeEffect.fireworkColorBallSourceRect =
        QRectF(0.0, 0.0, colorBallImage.width(), colorBallImage.height());
    state.playheadSeconds = clipTimeSeconds;
    return state;
}

PreviewFrameState buildOffsetStateAt(qreal clipTimeSeconds)
{
    PreviewFrameState state = buildStateAt(clipTimeSeconds);
    state.noteMarkers[0].touchPoint = QPointF(360.0, 180.0);
    return state;
}

bool verifySpreadDwellAndTail(QTextStream& err)
{
    const auto layerAt = [](qreal second) {
        const PreviewFrameState state = buildStateAt(second);
        return miacode::preview::scene::buildPreviewJudgeFireworkLayerState(
            state, miacode::preview::scene::PreviewActiveMarkerView(state.noteMarkers),
            QRectF(0.0, 0.0, 540.0, 540.0));
    };
    const auto onset = layerAt(0.0);
    bool sawLargeBall = false;
    bool sawSmallBallExit = false;
    bool sawRelease = false;
    for (int frame = 0; frame <= 120; ++frame) {
        const auto layer = layerAt(frame / 60.0);
        if (layer.active) {
            if (!require(std::isfinite(layer.outerRadius) && layer.outerRadius >= 0.0,
                         QStringLiteral("active firework has a finite radius"), err)) return false;
            sawLargeBall |= layer.drawColorBallBig;
            sawSmallBallExit |= layer.drawColorBallBig && !layer.drawColorBall;
        } else if (frame > 0) {
            sawRelease = true;
        }
    }
    return require(onset.active && onset.drawColorBall,
                   QStringLiteral("touch triggers the small ball"), err)
        && require(sawLargeBall && sawSmallBallExit && sawRelease,
                   QStringLiteral("firework expands, releases the small ball and finishes"), err);
}

bool verifyTouchTriggerClock(QTextStream& err)
{
    auto state = buildStateAt(2.0);
    state.noteMarkers[0].type = QStringLiteral("touch");
    state.noteMarkers[0].second = 2.0;
    const auto layer = miacode::preview::scene::buildPreviewJudgeFireworkLayerState(
        state, miacode::preview::scene::PreviewActiveMarkerView(state.noteMarkers),
        QRectF(0.0, 0.0, 540.0, 540.0));
    return require(layer.active && layer.drawColorBall,
                   QStringLiteral("touch hit triggers the small ball on the same frame"), err)
        && requireNear(layer.triggerSecond, 2.0, QStringLiteral("touch trigger second"), err);
}

bool verifyClipCenterStaysOnPlayfield(QTextStream& err)
{
    const PreviewFrameState state = buildOffsetStateAt(0.3);
    const PreviewJudgeFireworkLayerState layerState =
        miacode::preview::scene::buildPreviewJudgeFireworkLayerState(
            state,
            miacode::preview::scene::PreviewActiveMarkerView(state.noteMarkers),
            QRectF(0.0, 0.0, 540.0, 540.0)
        );
    if (!requireNear(layerState.center.x(), 360.0, QStringLiteral("offset firework center x"), err)) {
        return false;
    }
    if (!requireNear(layerState.center.y(), 180.0, QStringLiteral("offset firework center y"), err)) {
        return false;
    }
    if (!requireNear(layerState.clipCenter.x(), 270.0, QStringLiteral("stage clip center x"), err)) {
        return false;
    }
    if (!requireNear(layerState.clipCenter.y(), 270.0, QStringLiteral("stage clip center y"), err)) {
        return false;
    }
    if (!requireNear(layerState.clipRadius, 270.0,
                     QStringLiteral("firework fills the full canvas circle including the outer ring"), err)) {
        return false;
    }
    const qreal farEdgeDistance = layerState.clipRadius + std::hypot(90.0, 90.0);
    const auto scaledLayer = miacode::preview::scene::buildPreviewJudgeFireworkLayerState(
        state, miacode::preview::scene::PreviewActiveMarkerView(state.noteMarkers),
        QRectF(80.0, 40.0, 1080.0, 1080.0));
    return require(layerState.outerRadius > 0.0,
                       QStringLiteral("off-centre burst has a positive expansion radius"), err)
        && require(layerState.outerRadius < farEdgeDistance,
                   QStringLiteral("burst edge stays inside the far playfield boundary"), err)
        && requireNear(scaledLayer.clipRadius, 540.0,
                       QStringLiteral("full-circle firework scales with the export canvas"), err)
        && requireNear(scaledLayer.clipCenter.x(), 620.0,
                       QStringLiteral("scaled canvas clip center x"), err)
        && requireNear(scaledLayer.clipCenter.y(), 580.0,
                       QStringLiteral("scaled canvas clip center y"), err);
}

}  // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream err(stderr);
    QTextStream out(stdout);

    if (!verifyTouchTriggerClock(err)) {
        return 1;
    }
    if (!verifySpreadDwellAndTail(err)) {
        return 1;
    }
    if (!verifyClipCenterStaysOnPlayfield(err)) {
        return 1;
    }

    out << "preview_firework_lifecycle_spec ok" << Qt::endl;
    return 0;
}
