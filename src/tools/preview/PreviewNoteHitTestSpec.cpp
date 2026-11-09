#include "preview/quick_scene/PreviewNoteHitTest.h"

#include <QGuiApplication>
#include <QPainter>
#include <QDebug>

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    using namespace miacode::preview;
    int failures = 0;
    const auto expect = [&](bool condition, const char* label) {
        if (!condition) { ++failures; qWarning() << "FAIL:" << label; }
    };
    QImage image(40, 40, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    { QPainter paint(&image); paint.fillRect(10, 10, 20, 20, Qt::white); }
    scene::PreviewSpriteDescriptor sprite;
    sprite.image = &image; sprite.center = {100, 100}; sprite.width = sprite.height = 80;
    sprite.rotationDegrees = 45;
    selection::SpriteHitGeometry geometry;
    auto shape = geometry.shape(sprite);
    expect(shape.contains({100, 100}) && !shape.contains({140, 100}),
        "rotated sprite tests texture alpha rather than its bounding box");
    sprite.sourceRect = QRectF(10, 10, 20, 20);
    shape = geometry.shape(sprite);
    expect(shape.contains({140, 100}), "source cropping maps the rendered subimage to the sprite extent");
    sprite.opacity = 0;
    expect(geometry.shape(sprite).isEmpty(), "transparent lifecycle sprites cannot be selected");
    QVector<selection::NoteHitShape> hits{{11, shape}, {12, shape.translated(200, 0)}};
    expect(selection::notesIntersecting(hits, QRectF(90, 90, 20, 20)) == QSet<quint64>{11},
        "box selection intersects actual geometry and excludes other positions");
    expect(selection::notesIntersecting(hits, QRectF(110, 110, -20, -20)) == QSet<quint64>{11},
        "drag direction does not change selection");
    scene::PreviewFrameState state;
    state.skin.tapImage = image;
    TimelineNoteMarker marker;
    marker.second = 2.0; marker.type = QStringLiteral("tap"); marker.sourceLine = 3; marker.sourceCol = 7;
    state.noteMarkers = {marker}; state.playheadSeconds = 1.9;
    const auto visible = selection::visibleNoteHitShapes(state, {600, 600});
    expect(!visible.isEmpty() && visible.first().location == ((quint64(3) << 32) | 7),
        "a visible note is mapped to its source identity");
    state.playheadSeconds = 20;
    expect(selection::visibleNoteHitShapes(state, {600, 600}).isEmpty(), "expired notes are excluded");
    state.playheadSeconds = -20;
    expect(selection::visibleNoteHitShapes(state, {600, 600}).isEmpty(), "future notes outside their visual lifecycle are excluded");
    return failures == 0 ? 0 : 1;
}
