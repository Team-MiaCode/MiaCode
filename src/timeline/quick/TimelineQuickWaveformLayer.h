#pragma once

#include "timeline/TimelineSceneState.h"

class QSGNode;
class QQuickWindow;

class TimelineQuickWaveformLayer
{
public:
    QSGNode* updateNode(
        QSGNode* oldNode,
        const miacode::timeline::TimelineSceneState& state,
        QQuickWindow* window) const;
};
