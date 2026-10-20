#pragma once

#include <QColor>
#include <QElapsedTimer>
#include <QMetaObject>
#include <QPointer>
#include <QQuickPaintedItem>
#include <QtQml/qqmlregistration.h>
#include <QSize>
#include <memory>

#include "core/scene/PreviewLayerOrder.h"
#include "preview/quick_scene/PreviewQuickGraphicsInfo.h"

class QPainter;
class PreviewRuntime;
namespace miacode::preview::scene {
struct PreviewFrameState;
}

namespace miacode::preview::hud {

struct HudInformationLayoutCache;

// Phase 4f — standalone HUD painter, used by PreviewQuickHudLayer via
// QQuickPaintedItem::paint. All inputs are read-only; painter must be
// set up to draw within the canvas's local coordinate space (top-left
// origin, size canvasSize).
void paintPreviewHudOverlay(
    QPainter& painter,
    const miacode::preview::scene::PreviewFrameState& state,
    const QSize& canvasSize,
    miacode::preview::scene::PreviewRenderLayerFlags layerFlags
        = miacode::preview::scene::kPreviewAllRenderLayers,
    const miacode::preview::quick_scene::QuickGraphicsInfo& graphicsInfo = {},
    const QColor& textColor = QColor(Qt::white),
    const QColor& shadowColor = QColor(0, 0, 0, 190),
    HudInformationLayoutCache* informationLayoutCache = nullptr);

void paintCenterDisplay(
    QPainter& painter,
    const miacode::preview::scene::PreviewFrameState& state,
    const QSize& canvasSize);

}  // namespace miacode::preview::hud

class PreviewQuickHudLayer : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QObject* runtime READ runtimeObject WRITE setRuntimeObject NOTIFY runtimeChanged)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY textColorChanged)
    Q_PROPERTY(QColor shadowColor READ shadowColor WRITE setShadowColor NOTIFY shadowColorChanged)

public:
    explicit PreviewQuickHudLayer(QQuickItem* parent = nullptr);
    ~PreviewQuickHudLayer() override;

    void setRuntime(PreviewRuntime* runtime);
    QObject* runtimeObject() const;
    void setRuntimeObject(QObject* runtimeObject);
    void setFrameState(const miacode::preview::scene::PreviewFrameState* frameState);
    void setLayerFlags(miacode::preview::scene::PreviewRenderLayerFlags layerFlags);
    QColor textColor() const;
    void setTextColor(const QColor& color);
    QColor shadowColor() const;
    void setShadowColor(const QColor& color);
    void paint(QPainter* painter) override;

signals:
    void runtimeChanged();
    void textColorChanged();
    void shadowColorChanged();

private:
    void requestThrottledUpdate();

    QPointer<PreviewRuntime> runtime_;
    QMetaObject::Connection runtimeUpdateConnection_;
    const miacode::preview::scene::PreviewFrameState* frameState_ = nullptr;
    miacode::preview::scene::PreviewRenderLayerFlags layerFlags_ =
        miacode::preview::scene::kPreviewAllRenderLayers;
    QColor textColor_ = QColor(Qt::white);
    QColor shadowColor_ = QColor(0, 0, 0, 190);
    // The HUD draws diagnostic text (FPS, max frame interval, stutter
    // counts, timestamps) — none of which the human eye benefits from at
    // 60Hz update rate. Each `update()` on a QQuickPaintedItem rasterises
    // the full-area backing texture and re-uploads it during the QSG sync
    // phase, which dominates frame time for an item the size of the
    // preview surface. We throttle to ~kHudUpdateIntervalMs so the HUD
    // refresh rate is decoupled from frameStateChanged firings.
    QElapsedTimer hudUpdateThrottleTimer_;
    qint64 lastHudUpdateMs_ = -1;
    bool hudUpdatePending_ = false;
    QPointer<QQuickWindow> graphicsInfoWindow_;
    miacode::preview::quick_scene::QuickGraphicsInfo graphicsInfo_;
    bool graphicsInfoReady_ = false;
    std::unique_ptr<miacode::preview::hud::HudInformationLayoutCache> informationLayoutCache_;
};
