#include "MobileVideoFrameRouter.h"
#include <QSignalBlocker>

namespace miacode::android {
MobileVideoFrameRouter::MobileVideoFrameRouter(QObject* parent) : QObject(parent) {
    clock_.start();
    timer_.setTimerType(Qt::PreciseTimer);
    timer_.setInterval(cadence_.pollingIntervalMs());
    connect(&timer_, &QTimer::timeout, this, [this] { present(); });
    connect(&input_, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame& frame) {
        latest_ = frame;
        pending_ = true;
        // Paused seeks must update immediately, even when the playback timer
        // is stopped. Playing frames are coalesced before reaching VideoOutput.
        if (!playing_) present(true);
    });
}
void MobileVideoFrameRouter::attach(QVideoSink* primary, QVideoSink* inner) {
    if (primary_ == primary && inner_ == inner) return;
    const bool sameRhi = input_.rhi() == (primary ? primary->rhi() : nullptr);
    const QVideoFrame retained = sameRhi ? latest_ : QVideoFrame();
    // Frames from a retired render context must not outlive its RHI.
    clear();
    primary_ = primary;
    inner_ = inner == primary ? nullptr : inner;
    input_.setRhi(primary ? primary->rhi() : nullptr);
    if (retained.isValid()) { latest_ = retained; pending_ = true; present(true); }
}
void MobileVideoFrameRouter::detach() {
    watchWindow(nullptr);
    clear();
    primary_.clear();
    inner_.clear();
    input_.setRhi(nullptr);
}
void MobileVideoFrameRouter::watchWindow(QQuickWindow* window) {
    if (window_ == window) return;
    QObject::disconnect(rhiInitialized_);
    QObject::disconnect(rhiInvalidated_);
    QObject::disconnect(windowDestroyed_);
    window_ = window;
    input_.setRhi(window && primary_ ? primary_->rhi() : nullptr);
    if (!window) return;
    // Match Qt 6.11 QQuickVideoOutput's render-context lifetime. Updating only
    // the backend RHI here avoids touching GUI timers or the pending-frame
    // queue on the render thread. VideoOutput installs its own sink's RHI
    // before these connections, since it joined the window before attach.
    rhiInitialized_ = connect(window, &QQuickWindow::sceneGraphInitialized, this,
        [this] { input_.setRhi(primary_ ? primary_->rhi() : nullptr); }, Qt::DirectConnection);
    rhiInvalidated_ = connect(window, &QQuickWindow::sceneGraphInvalidated, this,
        [this] { input_.setRhi(nullptr); }, Qt::DirectConnection);
    windowDestroyed_ = connect(window, &QObject::destroyed, this,
        [this] { input_.setRhi(nullptr); }, Qt::DirectConnection);
}
void MobileVideoFrameRouter::clear() {
    latest_ = QVideoFrame();
    pending_ = false;
    cadence_.reset();
    const QSignalBlocker block(&input_);
    input_.setVideoFrame(QVideoFrame());
    if (primary_) primary_->setVideoFrame(QVideoFrame());
    if (inner_) inner_->setVideoFrame(QVideoFrame());
    emit framePresented(false);
}
void MobileVideoFrameRouter::setPlaying(bool playing) {
    playing_ = playing;
    cadence_.reset();
    if (playing) timer_.start();
    else { timer_.stop(); present(true); }
}
void MobileVideoFrameRouter::setFrameRate(PreviewCanvasFrameRateMode mode, double refreshHz) {
    cadence_.configure(mode, refreshHz);
    timer_.setInterval(cadence_.pollingIntervalMs());
}
void MobileVideoFrameRouter::present(bool force) {
    if (!primary_ || !pending_) return;
    if (!force && !cadence_.due(clock_.nsecsElapsed())) return;
    pending_ = false;
    primary_->setVideoFrame(latest_);
    if (inner_) inner_->setVideoFrame(latest_);
    emit framePresented(latest_.isValid());
}
}
