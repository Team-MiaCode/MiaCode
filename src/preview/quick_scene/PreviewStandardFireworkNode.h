#pragma once

#include "core/scene/PreviewFrameState.h"
#include "core/scene/PreviewJudgeFireworkLayerState.h"

class QQuickWindow;
class QSGNode;
class PreviewTextureRepository;

QSGNode* updatePreviewStandardFireworkNode(
    QSGNode* oldNode,
    const miacode::preview::scene::PreviewFrameState& state,
    const miacode::preview::scene::PreviewJudgeFireworkLayerState& layerState,
    const QRectF& stageRect,
    QQuickWindow* window,
    PreviewTextureRepository* textures);
