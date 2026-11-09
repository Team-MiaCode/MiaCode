#include "core/scene/PreviewFrameState.h"
#include "preview/quick_scene/PreviewQuickSceneRoot.h"

#include <QGuiApplication>
#include <QQuickWindow>
#include <QSGNode>
#include <QSGRendererInterface>
#include <QTest>
#include <QTextStream>

#include <atomic>

namespace {

class SceneProbe final : public PreviewQuickSceneRoot
{
public:
    using PreviewQuickSceneRoot::PreviewQuickSceneRoot;
    std::atomic_bool hasMaimuriJudgeNode = false;

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override
    {
        QSGNode* root = PreviewQuickSceneRoot::updatePaintNode(oldNode, data);
        constexpr int kMaimuriJudgeSlot = 18;
        const auto* slot = root != nullptr && root->childCount() > kMaimuriJudgeSlot
            ? root->childAtIndex(kMaimuriJudgeSlot) : nullptr;
        hasMaimuriJudgeNode.store(slot != nullptr && slot->firstChild() != nullptr);
        return root;
    }
};

} // namespace

int main(int argc, char** argv)
{
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    QGuiApplication app(argc, argv);
    miacode::preview::scene::PreviewFrameState frame;
    frame.playheadSeconds = 0.10;
    frame.muriRenderOptions.renderMode = RenderMode::MaimuriDxStyle;
    frame.judgeOverlay.simpleText.good.image = QImage(32, 16, QImage::Format_ARGB32_Premultiplied);
    frame.judgeOverlay.simpleText.good.image.fill(Qt::white);

    TimelineNoteMarker tap;
    tap.type = QStringLiteral("tap");
    tap.lane = 1;
    frame.noteMarkers.append(tap);
    MuriJudgeSpriteEvent judgment;
    judgment.pad = QStringLiteral("A1");
    judgment.markerKey = makeMarkerAnalysisKey(tap);
    frame.muriAnalysisReport.judgeSpriteEvents.append(judgment);

    QQuickWindow window;
    window.resize(480, 480);
    SceneProbe root(window.contentItem());
    root.setSize(QSizeF(480, 480));
    root.setLayerFlags(miacode::preview::scene::MaimuriDxJudgeLayer);
    root.setFrameState(&frame);
    window.show();
    const bool ready = QTest::qWaitFor([&] { return root.hasMaimuriJudgeNode.load(); }, 3000);
    if (!ready) {
        QTextStream(stderr) << "Maimuri DX judge text did not reach the scene graph of the explicit-frame preview." << Qt::endl;
    }
    window.hide();
    return ready ? 0 : 1;
}
