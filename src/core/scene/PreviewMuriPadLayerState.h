#pragma once

#include <QStringList>

#include "core/scene/PreviewCircleDescriptor.h"
#include "core/scene/PreviewFrameState.h"

namespace miacode::preview::scene {

struct PreviewMuriPadLayerCache {
    QStringList insertionOrder;
    QStringList drawOrder;
    qsizetype sourceWindowCount = -1;
};

struct PreviewMuriPadLayerState {
    PreviewCircleDescriptors circles;
};

PreviewMuriPadLayerState buildPreviewMuriPadLayerState(
    const PreviewFrameState& state,
    const QRectF& playfieldRect,
    const QVector<int>& visibleIndices,
    PreviewMuriPadLayerCache& cache
);

}  // namespace miacode::preview::scene
