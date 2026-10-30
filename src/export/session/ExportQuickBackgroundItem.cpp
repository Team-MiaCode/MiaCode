#include "export/session/ExportQuickBackgroundItem.h"

#include "preview/quick_scene/PreviewQuickSceneRoot.h"
#include <QSGSimpleRectNode>

ExportQuickBackgroundItem::ExportQuickBackgroundItem(QQuickItem* parent)
    : QQuickItem(parent), stage_(new PreviewQuickSceneRoot(this))
{
    setFlag(ItemHasContents);
    stage_->setLayerFlags(miacode::preview::scene::StageBackgroundLayer);
    stage_->setFrameState(&state_);
}

void ExportQuickBackgroundItem::setFrameState(
    const miacode::preview::scene::PreviewFrameState& state, bool fullFrame, qreal brightness)
{
    state_.render = state.render;
    state_.render.centerDisplayMode = miacode::preview_gameplay::CenterDisplayMode::Off;
    state_.assets = state.assets;
    state_.media = state.media;
    if (fullFrame) {
        state_.render.backgroundBrightnessInner = 1.0;
        state_.render.backgroundBrightnessOuter = 1.0;
        state_.render.backgroundScaleMode = PreviewBackgroundScaleMode::FillCrop;
    }
    stage_->setBackgroundColor(state.media.stageMediaAvailable ? QColor(Qt::black)
                                                              : QColor(QStringLiteral("#1F2833")));
    stage_->setOpacity(brightness);
    stage_->update();
    update();
}

void ExportQuickBackgroundItem::geometryChange(const QRectF& geometry, const QRectF& previous)
{
    QQuickItem::geometryChange(geometry, previous);
    stage_->setSize(geometry.size());
    update();
}

QSGNode* ExportQuickBackgroundItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    auto* fill = oldNode ? static_cast<QSGSimpleRectNode*>(oldNode) : new QSGSimpleRectNode;
    fill->setRect(boundingRect());
    fill->setColor(Qt::black);
    return fill;
}
