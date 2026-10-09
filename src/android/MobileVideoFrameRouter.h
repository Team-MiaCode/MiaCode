#pragma once

#include "MobileFrameCadence.h"
#include <QElapsedTimer>
#include <QPointer>
#include <QTimer>
#include <QVideoSink>
#include <QVideoFrame>
#include <QQuickWindow>

namespace miacode::android {

// The decoder writes only to input_. Both v2 PV surfaces receive the same
// newest frame on their own presentation cadence, retaining the Qt RHI path.
class MobileVideoFrameRouter final : public QObject {
    Q_OBJECT
public:
    explicit MobileVideoFrameRouter(QObject* parent = nullptr);
    QVideoSink* input() { return &input_; }
    void attach(QVideoSink* primary, QVideoSink* inner);
    void detach();
    void clear();
    void setPlaying(bool playing);
    void setFrameRate(PreviewCanvasFrameRateMode mode, double refreshHz);
    void watchWindow(QQuickWindow* window);
signals:
    void framePresented(bool valid);
private:
    void present(bool force = false);
    QVideoSink input_;
    QPointer<QVideoSink> primary_, inner_;
    QVideoFrame latest_;
    QTimer timer_;
    QElapsedTimer clock_;
    MobileFrameCadence cadence_;
    bool playing_ = false;
    bool pending_ = false;
    QPointer<QQuickWindow> window_;
    QMetaObject::Connection rhiInitialized_, rhiInvalidated_, windowDestroyed_;
};
}
