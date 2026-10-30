#include "core/scene/PreviewMuriPadLayerState.h"

#include "core/analysis/MuriConfig.h"
#include "core/scene/PreviewSceneMath.h"

#include <QHash>

namespace miacode::preview::scene {

PreviewMuriPadLayerState buildPreviewMuriPadLayerState(
    const PreviewFrameState& state,
    const QRectF& playfieldRect,
    const QVector<int>& visibleIndices,
    PreviewMuriPadLayerCache& cache
)
{
    PreviewMuriPadLayerState layerState;
    if (!state.muriRenderOptions.showJudgeMarkers) {
        cache = {};
        return layerState;
    }
    QHash<QString, int> activePadCounts;
    activePadCounts.reserve(33);
    QStringList insertionOrder;
    for (int index : visibleIndices) {
        const MuriPadWindow& window = state.muriAnalysisReport.padWindows[index];
        if (window.startSecond <= state.playheadSeconds + 1e-6
            && window.endSecond + 1e-6 >= state.playheadSeconds) {
            int& count = activePadCounts[window.pad];
            if (count == 0) insertionOrder.append(window.pad);
            count += 1;
        }
    }
    if (activePadCounts.isEmpty()) {
        return layerState;
    }

    if (cache.sourceWindowCount != state.muriAnalysisReport.padWindows.size()
        || cache.insertionOrder != insertionOrder) {
        // Preserve the original hash order for overlapping translucent pads.
        // Its large bucket table is needed only when the insertion order changes.
        QHash<QString, int> orderedPads;
        orderedPads.reserve(state.muriAnalysisReport.padWindows.size());
        for (const QString& pad : insertionOrder) orderedPads.insert(pad, 0);
        cache.drawOrder.clear();
        for (auto it = orderedPads.cbegin(); it != orderedPads.cend(); ++it) {
            cache.drawOrder.append(it.key());
        }
        cache.sourceWindowCount = state.muriAnalysisReport.padWindows.size();
        cache.insertionOrder = std::move(insertionOrder);
    }

    layerState.circles.reserve(activePadCounts.size());
    const bool maimuriDxStyle = state.muriRenderOptions.renderMode == RenderMode::MaimuriDxStyle;
    for (const QString& pad : cache.drawOrder) {
        const int count = activePadCounts.value(pad);
        const QPointF logicalCenter = miacode::muri::padCenter(pad);
        const qreal logicalRadius = static_cast<qreal>(miacode::muri::padRadius(pad));
        if (logicalRadius <= 0.0) {
            continue;
        }

        PreviewCircleDescriptor circle;
        circle.center = mapLogicalPointToRect(logicalCenter, playfieldRect);
        circle.radiusX = qMax<qreal>(2.0, mapLogicalLengthToRect(logicalRadius, playfieldRect));
        circle.radiusY = circle.radiusX;
        if (maimuriDxStyle) {
            circle.fillColor = QColor(255, 255, 0);
        } else {
            circle.fillColor = QColor(255, 225, 84, qBound(90, 110 + count * 26, 200));
            circle.strokeColor = QColor(255, 248, 187, qBound(130, 170 + count * 18, 255));
            circle.strokeWidth = qMax<qreal>(2.0, circle.radiusX * 0.08);
        }
        layerState.circles.append(circle);
    }

    return layerState;
}

}  // namespace miacode::preview::scene
