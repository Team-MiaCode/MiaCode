#pragma once

#include "core/scene/PreviewHeadLayerState.h"
#include "core/scene/PreviewTrackLayerState.h"
#include "core/scene/PreviewSlideMotionLayerState.h"
#include "core/scene/PreviewTouchLayerState.h"
#include "core/scene/PreviewTouchHoldLayerState.h"
#include "core/scene/PreviewSceneGeometry.h"

#include <QBitmap>
#include <QPainterPath>
#include <QRegion>
#include <QTransform>

namespace miacode::preview::selection {

struct NoteHitShape {
    quint64 location = 0;
    QPainterPath shape;
};

// Hit testing follows rendered texture alpha, source cropping, transforms and
// opacity. A bounding box over a rotated Slide would select empty corners.
class SpriteHitGeometry {
public:
    QPainterPath shape(const scene::PreviewSpriteDescriptor& sprite)
    {
        if (!sprite.image || sprite.image->isNull() || sprite.opacity <= 0.0
            || sprite.width <= 0.0 || sprite.height <= 0.0) return {};
        const auto key = sprite.image->cacheKey();
        auto it = alpha_.find(key);
        if (it == alpha_.end()) {
            QPainterPath alpha;
            const QRegion region(QBitmap::fromImage(sprite.image->createAlphaMask()));
            for (const QRect& rect : region) alpha.addRect(rect);
            it = alpha_.insert(key, alpha.simplified());
        }
        const QRectF source = sprite.sourceRect.isValid() && !sprite.sourceRect.isEmpty()
            ? sprite.sourceRect : QRectF(sprite.image->rect());
        QPainterPath crop;
        crop.addRect(source);
        QTransform transform;
        transform.translate(sprite.center.x(), sprite.center.y());
        transform.rotate(sprite.rotationDegrees);
        transform.scale(sprite.width / source.width(), sprite.height / source.height());
        transform.translate(-source.center().x(), -source.center().y());
        return transform.map(it.value().intersected(crop));
    }
private:
    QHash<qint64, QPainterPath> alpha_;
};

inline QVector<NoteHitShape> visibleNoteHitShapes(const scene::PreviewFrameState& state, const QSize& size)
{
    using namespace scene;
    PreviewPreparedSceneCache prepared;
    prepared.sync(state);
    QSet<int> active;
    const auto collect = [&](const PreviewPreparedLayerWindow<PreviewPreparedMarkerEntry>& layer) {
        PreviewLayerWindowCursor cursor;
        syncPreviewLayerWindowCursor(layer, state.playheadSeconds, &cursor);
        for (int index : cursor.activePreparedIndices) active.insert(layer.entries[index].markerIndex);
    };
    collect(prepared.headLayer()); collect(prepared.slideLikeLayer());
    collect(prepared.touchLayer()); collect(prepared.touchHoldLayer());
    const QRectF field = playfieldRectForStage(stageRectForSize(size), state.render.layoutSquareScale);
    QPainterPath viewport;
    viewport.addRect(QRectF(QPointF{}, size));
    SpriteHitGeometry geometry;
    QVector<NoteHitShape> result;
    PreviewHeadRenderAssetCache headCache;
    for (int index : active) {
        const auto& marker = state.noteMarkers[index];
        const QVector<TimelineNoteMarker> one{marker};
        const PreviewActiveMarkerView view(one);
        QPainterPath shape;
        shape.setFillRule(Qt::WindingFill);
        const auto append = [&](const PreviewSpriteDescriptors& sprites) {
            for (const auto& sprite : sprites) shape.addPath(geometry.shape(sprite));
        };
        append(buildPreviewHeadLayerState(state, view, field, &headCache).sprites);
        append(buildPreviewTrackLayerState(state, view, field).sprites);
        append(buildPreviewSlideMotionLayerState(state, view, field).sprites);
        append(buildPreviewTouchLayerState(state, view, field).sprites);
        const auto hold = buildPreviewTouchHoldLayerState(state, view, field);
        append(hold.sprites);
        for (const auto& arc : hold.arcs) {
            PreviewSpriteDescriptor sprite;
            sprite.image = arc.image; sprite.center = arc.center;
            sprite.width = arc.width; sprite.height = arc.height; sprite.opacity = arc.opacity;
            QPainterPath wedge;
            const QRectF rect(arc.center.x() - arc.width / 2, arc.center.y() - arc.height / 2, arc.width, arc.height);
            wedge.moveTo(arc.center);
            wedge.arcTo(rect, arc.startDegrees, arc.sweepDegrees);
            wedge.closeSubpath();
            shape.addPath(geometry.shape(sprite).intersected(wedge));
        }
        shape = shape.intersected(viewport);
        if (!shape.isEmpty()) result.append({
            (quint64(quint32(marker.sourceLine)) << 32) | quint32(marker.sourceCol), shape});
    }
    return result;
}

inline QSet<quint64> notesIntersecting(const QVector<NoteHitShape>& shapes, const QRectF& selection)
{
    QSet<quint64> locations;
    QPainterPath area;
    area.addRect(selection.normalized());
    for (const auto& hit : shapes)
        if (hit.shape.intersects(area)) locations.insert(hit.location);
    return locations;
}
} // namespace miacode::preview::selection
