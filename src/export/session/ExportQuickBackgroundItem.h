#pragma once

#include <QQuickItem>
#include "core/scene/PreviewFrameState.h"

class PreviewQuickSceneRoot;

// Shared export background under the chart and intro overlay.
class ExportQuickBackgroundItem final : public QQuickItem
{
public:
    explicit ExportQuickBackgroundItem(QQuickItem* parent);
    void setFrameState(const miacode::preview::scene::PreviewFrameState& state,
                       bool fullFrame, qreal brightness);

protected:
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;

private:
    miacode::preview::scene::PreviewFrameState state_;
    PreviewQuickSceneRoot* stage_;
};
