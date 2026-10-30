#pragma once

#include "core/scene/PreviewMuriPadLayerState.h"

class QQuickWindow;
class QSGNode;
class PreviewTextureRepository;

class PreviewQuickMuriPadLayer
{
public:
    QSGNode* updateNode(
        QSGNode* oldNode,
        const miacode::preview::scene::PreviewFrameState& state,
        const QVector<int>& visibleIndices,
        const QSize& renderSize,
        QQuickWindow* window,
        PreviewTextureRepository* textures
    ) const;

private:
    mutable miacode::preview::scene::PreviewMuriPadLayerCache cache_;
};
