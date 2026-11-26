#include "timeline/quick/TimelineQuickGridLinesLayer.h"

#include <QMatrix4x4>

#include <QSGClipNode>
#include <QSGNode>
#include <QSGTransformNode>

#include "timeline/quick/TimelineQuickLayerUtils.h"

namespace {

struct TimelineQuickGridLinesRootNode : public QSGNode {
    quint64 gridRevision = 0;
    quint64 layoutRevision = 0;
    quint64 appearanceRevision = 0;
    int audioVisualizationMode = -1;
};

void clearChildren(QSGNode* node)
{
    if (node == nullptr) {
        return;
    }
    while (QSGNode* child = node->firstChild()) {
        node->removeChildNode(child);
        delete child;
    }
}

QSGClipNode* ensureClipRoot(TimelineQuickGridLinesRootNode* root)
{
    if (root == nullptr) {
        return nullptr;
    }
    auto* clipRoot = dynamic_cast<QSGClipNode*>(root->firstChild());
    if (clipRoot != nullptr) {
        return clipRoot;
    }
    if (QSGNode* child = root->firstChild(); child != nullptr) {
        root->removeChildNode(child);
        delete child;
    }
    clipRoot = new QSGClipNode();
    clipRoot->setIsRectangular(true);
    root->appendChildNode(clipRoot);
    return clipRoot;
}

QSGTransformNode* ensureTransformRoot(QSGClipNode* clipRoot)
{
    if (clipRoot == nullptr) {
        return nullptr;
    }
    auto* transformRoot = dynamic_cast<QSGTransformNode*>(clipRoot->firstChild());
    if (transformRoot != nullptr) {
        return transformRoot;
    }
    if (QSGNode* child = clipRoot->firstChild(); child != nullptr) {
        clipRoot->removeChildNode(child);
        delete child;
    }
    transformRoot = new QSGTransformNode();
    transformRoot->appendChildNode(new QSGNode());
    clipRoot->appendChildNode(transformRoot);
    return transformRoot;
}

QSGNode* ensureContentRoot(QSGTransformNode* transformRoot)
{
    if (transformRoot == nullptr) {
        return nullptr;
    }
    QSGNode* contentRoot = transformRoot->firstChild();
    if (contentRoot == nullptr) {
        contentRoot = new QSGNode();
        transformRoot->appendChildNode(contentRoot);
    }
    return contentRoot;
}

}  // namespace

QSGNode* TimelineQuickGridLinesLayer::updateNode(
    QSGNode* oldNode,
    const miacode::timeline::TimelineSceneState& state,
    QQuickWindow* window) const
{
    Q_UNUSED(window);
    auto* root = dynamic_cast<TimelineQuickGridLinesRootNode*>(oldNode);
    if (root == nullptr) {
        delete oldNode;
        root = new TimelineQuickGridLinesRootNode();
    }

    QSGClipNode* clipRoot = ensureClipRoot(root);
    QSGTransformNode* transformRoot = ensureTransformRoot(clipRoot);
    QSGNode* contentRoot = ensureContentRoot(transformRoot);

    // Clip to the timeline content rect (excludes the lane-number sidebar
    // and the header band). Mirrors what TimelineQuickHeaderLayer's
    // gridClipRoot used to do.
    const QRectF timelineClipRect(
        state.timelineLeft,
        state.timelineTop,
        qMax(0, state.viewportSize.width() - state.timelineLeft),
        state.timelineHeight);
    if (clipRoot != nullptr) {
        clipRoot->setClipRect(timelineClipRect);
    }

    // Playback follow produces a fractional scroll value every presented frame. Grid,
    // waveform and notes must consume that same value or their relative positions oscillate.
    QMatrix4x4 transform;
    transform.translate(-static_cast<float>(state.horizontalScrollValue), 0.0f);
    if (transformRoot != nullptr) {
        transformRoot->setMatrix(transform);
    }

    const bool spectrogram = state.audioVisualizationMode
        == static_cast<int>(miacode::timeline::AudioVisualizationMode::Spectrogram);
    const bool appearanceChanged = root->appearanceRevision != state.appearanceRevision;
    if (appearanceChanged
        || root->layoutRevision != state.layoutRevision
        || root->gridRevision != state.gridRevision
        || root->audioVisualizationMode != state.audioVisualizationMode) {
        clearChildren(contentRoot);
        // Same per-color batching as the previous in-headerLayer path.
        // The Blending=true override in TimelineQuickFlatColorBatchBuilder
        // applies here too, ensuring even alpha-255 bar lines stay on
        // the translucent draw path.
        TimelineQuickFlatColorBatchBuilder gridBatch(contentRoot);
        const auto appendLine = [&](const miacode::timeline::TimelineSceneLine& line) {
            if (!gridBatch.tryAppendOrthogonalLine(line)) {
                gridBatch.flush();
                contentRoot->appendChildNode(buildTimelineLineNode(line));
            }
        };
        if (spectrogram) {
            // Heat runs from near-black to near-white, so the palette gray
            // disappears on one end or the other. A dark rim plus a light core
            // stays readable on both. Measure lines stay the clearest, subdivision
            // lines are fainter, and comma lines are the lightest.
            const auto paintLine = [&](const miacode::timeline::TimelineSceneLine& line, bool rim) {
                miacode::timeline::TimelineSceneLine painted = line;
                const bool measure = line.width >= 1.25;
                const qreal height = line.end.y() - line.start.y();
                const bool subdivision = !measure
                    && state.timelineHeight > 1.0
                    && height >= state.timelineHeight * 0.83;
                if (rim) {
                    painted.width = line.width + 1.0;
                    painted.color = measure ? QColor(6, 8, 14, 150)
                        : subdivision ? QColor(6, 8, 14, 88)
                                      : QColor(6, 8, 14, 56);
                } else {
                    painted.color = measure ? QColor(255, 248, 236, 188)
                        : subdivision ? QColor(255, 248, 236, 112)
                                      : QColor(255, 248, 236, 72);
                }
                appendLine(painted);
            };
            for (const auto& line : state.gridLines) {
                paintLine(line, true);
            }
            gridBatch.flush();
            for (const auto& line : state.gridLines) {
                paintLine(line, false);
            }
        } else {
            for (const auto& line : state.gridLines) {
                appendLine(line);
            }
        }
        gridBatch.flush();
        root->gridRevision = state.gridRevision;
        root->audioVisualizationMode = state.audioVisualizationMode;
    }
    root->layoutRevision = state.layoutRevision;
    root->appearanceRevision = state.appearanceRevision;
    return root;
}
