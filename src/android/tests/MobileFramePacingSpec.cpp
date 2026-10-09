#include "android/MobileVideoFrameRouter.h"
#include <QGuiApplication>
#include <QImage>
#include <QEventLoop>
#include <QDebug>

using namespace miacode;
using namespace miacode::android;

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    bool ok = true;
    auto require = [&](bool value, const char* message) { if (!value) { qCritical() << message; ok = false; } };
    for (const auto mode : {PreviewCanvasFrameRateMode::Fps30, PreviewCanvasFrameRateMode::Fps60,
            PreviewCanvasFrameRateMode::Fps120, PreviewCanvasFrameRateMode::DisplayRefresh}) {
        MobileFrameCadence cadence;
        cadence.configure(mode, 120);
        int count = 0;
        for (qint64 ms = 0; ms < 1000; ++ms) count += cadence.due(ms * 1000000);
        const int expected = mode == PreviewCanvasFrameRateMode::Fps30 ? 30
            : mode == PreviewCanvasFrameRateMode::Fps60 ? 60 : 120;
        require(count == expected, "one second cadence must match requested display rate");
        require(cadence.due(5000000000), "delayed consumer must present latest once");
        require(!cadence.due(5000000000), "missed frames must not replay in a burst");
        cadence.reset();
        require(cadence.due(5000000000), "reset permits immediate seek frame");
    }
    MobileFrameCadence capped;
    capped.configure(PreviewCanvasFrameRateMode::Fps120, 60);
    require(capped.intervalNs() == 16666667, "120 mode is capped to actual 60Hz panel");
    capped.configure(PreviewCanvasFrameRateMode::DisplayRefresh, 0);
    require(capped.intervalNs() == 16666667, "unknown refresh has safe 60Hz fallback");

    MobileVideoFrameRouter router;
    QVideoSink primary, inner, fullscreen;
    router.attach(&primary, &inner);
    QImage red(16, 16, QImage::Format_RGBA8888); red.fill(Qt::red);
    QImage green(16, 16, QImage::Format_RGBA8888); green.fill(Qt::green);
    QImage blue(16, 16, QImage::Format_RGBA8888); blue.fill(Qt::blue);
    auto color = [](QVideoSink& sink) { return sink.videoFrame().toImage().pixelColor(0, 0); };
    router.input()->setVideoFrame(QVideoFrame(red));
    require(color(primary) == QColor(Qt::red) && color(inner) == QColor(Qt::red), "paused seek is immediately visible on both actual sinks");
    router.setFrameRate(PreviewCanvasFrameRateMode::Fps30, 60);
    router.setPlaying(true);
    int delivered = 0;
    QObject::connect(&primary, &QVideoSink::videoFrameChanged, &app, [&](const QVideoFrame& f) { if (f.isValid()) ++delivered; });
    router.input()->setVideoFrame(QVideoFrame(green));
    router.input()->setVideoFrame(QVideoFrame(blue));
    require(color(primary) == QColor(Qt::red), "decoder must not bypass presentation cadence");
    QEventLoop wait;
    QTimer::singleShot(50, &wait, &QEventLoop::quit);
    wait.exec();
    require(delivered == 1 && color(primary) == QColor(Qt::blue) && color(inner) == QColor(Qt::blue), "playing burst coalesces into newest frame for both sinks");
    router.setPlaying(false);
    router.attach(&fullscreen, nullptr);
    require(color(fullscreen) == QColor(Qt::blue), "same-context fullscreen retains paused PV");
    require(!primary.videoFrame().isValid() && !inner.videoFrame().isValid(), "retired output frame ownership is released");
    router.clear();
    require(!fullscreen.videoFrame().isValid() && !router.input()->videoFrame().isValid(), "source replacement releases presentation and decoder frames");
    router.detach();
    router.input()->setVideoFrame(QVideoFrame(red));
    require(!fullscreen.videoFrame().isValid(), "detached output does not receive late frames");
    return ok ? 0 : 1;
}
