#pragma once

class QSGNode;
class QQuickWindow;

namespace miacode::timeline {
struct TimelineSceneState;
}

class TimelineQuickSpectrogramLayer
{
public:
    QSGNode* updateNode(
        QSGNode* oldNode,
        const miacode::timeline::TimelineSceneState& state,
        QQuickWindow* window) const;
};
