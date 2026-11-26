#include "timeline/quick/TimelineQuickSpectrogramLayer.h"

#include <QQuickWindow>
#include <QSGClipNode>
#include <QSGSimpleTextureNode>
#include <QSGTransformNode>

#include "timeline/TimelineSceneState.h"

namespace {

struct TimelineQuickSpectrogramRootNode : public QSGNode {
    quint64 revision = 0;
    quint64 layoutRevision = 0;
    quint64 appearanceRevision = 0;
    qreal devicePixelRatio = 0.0;
};

QSGClipNode* ensureClipRoot(TimelineQuickSpectrogramRootNode* root)
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
    clipRoot->appendChildNode(transformRoot);
    return transformRoot;
}

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

}  // namespace

QSGNode* TimelineQuickSpectrogramLayer::updateNode(
    QSGNode* oldNode,
    const miacode::timeline::TimelineSceneState& state,
    QQuickWindow* window) const
{
    auto* root = dynamic_cast<TimelineQuickSpectrogramRootNode*>(oldNode);
    if (root == nullptr) {
        delete oldNode;
        root = new TimelineQuickSpectrogramRootNode();
    }
    QSGClipNode* clipRoot = ensureClipRoot(root);
    QSGTransformNode* transformRoot = ensureTransformRoot(clipRoot);
    const qreal devicePixelRatio = window != nullptr
        ? qMax<qreal>(1.0, window->effectiveDevicePixelRatio()) : 1.0;
    if (clipRoot != nullptr) {
        clipRoot->setClipRect(QRectF(
            state.timelineLeft,
            state.timelineTop,
            qMax<qreal>(0.0, state.viewportSize.width() - state.timelineLeft),
            state.timelineHeight));
    }
    if (transformRoot != nullptr) {
        QMatrix4x4 matrix;
        matrix.translate(-static_cast<float>(state.horizontalScrollValue), 0.0f);
        transformRoot->setMatrix(matrix);
    }
    if (root->revision == state.waveformRevision
        && root->layoutRevision == state.layoutRevision
        && root->appearanceRevision == state.appearanceRevision
        && qFuzzyCompare(root->devicePixelRatio, devicePixelRatio)) {
        return root;
    }
    clearChildren(transformRoot);
    if (window != nullptr && state.spectrogram.valid && !state.spectrogram.image.isNull()) {
        QSGTexture* texture = window->createTextureFromImage(state.spectrogram.image);
        if (texture != nullptr) {
            auto* imageNode = new QSGSimpleTextureNode();
            imageNode->setOwnsTexture(true);
            imageNode->setFiltering(QSGTexture::Linear);
            imageNode->setTexture(texture);
            imageNode->setRect(state.spectrogram.rect);
            transformRoot->appendChildNode(imageNode);
        }
    }
    root->revision = state.waveformRevision;
    root->layoutRevision = state.layoutRevision;
    root->appearanceRevision = state.appearanceRevision;
    root->devicePixelRatio = devicePixelRatio;
    return root;
}
