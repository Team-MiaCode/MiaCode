#include "export/cover_export/CoverPvFrameSource.h"
#include "export/video_export/VideoExportController.h"

#include <QCoreApplication>
#include <QColor>
#include <QElapsedTimer>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextStream>

#include <functional>

namespace {

bool require(bool condition, const QString& message, QTextStream& err)
{
    if (!condition) {
        err << "FAIL: " << message << Qt::endl;
    }
    return condition;
}

// 1 s of red then 1 s of blue at 320x180, so a frame's colour says which
// second it was decoded from.
bool writeTestPv(const QString& ffmpegPath, const QString& path, QTextStream& err)
{
    QProcess process;
    process.start(ffmpegPath, {
        QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("error"), QStringLiteral("-y"),
        QStringLiteral("-filter_complex"),
        QStringLiteral("color=c=red:s=320x180:r=30:d=1[a];color=c=blue:s=320x180:r=30:d=1[b];"
                       "[a][b]concat=n=2:v=1:a=0,format=yuv420p[v]"),
        QStringLiteral("-map"), QStringLiteral("[v]"),
        QStringLiteral("-c:v"), QStringLiteral("mpeg4"), QStringLiteral("-q:v"), QStringLiteral("2"),
        path,
    });
    if (!process.waitForFinished(30000) || process.exitCode() != 0) {
        err << "FAIL: could not write the test PV: " << process.readAllStandardError() << Qt::endl;
        return false;
    }
    return true;
}

bool isRed(const QImage& image)
{
    const QColor c = image.pixelColor(image.width() / 2, image.height() / 2);
    return c.red() > 200 && c.blue() < 60;
}

bool isBlue(const QImage& image)
{
    const QColor c = image.pixelColor(image.width() / 2, image.height() / 2);
    return c.blue() > 200 && c.red() < 60;
}

bool waitFor(const std::function<bool()>& done, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (!done() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    return done();
}

}  // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    QTextStream err(stderr);
    const QString ffmpegPath = VideoExportController::ffmpegExecutablePath();
    if (ffmpegPath.isEmpty()) {
        out << "cover_pv_frame_source_spec skipped: no ffmpeg" << Qt::endl;
        return 0;
    }
    QTemporaryDir tempDir;
    const QString pvPath = tempDir.filePath(QStringLiteral("pv.mp4"));
    if (!writeTestPv(ffmpegPath, pvPath, err)) {
        return 1;
    }

    using miacode::cover_export::CoverPvFrameSource;
    QString error;
    const QImage red = CoverPvFrameSource::decodeFrame(pvPath, 0.5, 512, &error);
    bool ok = require(!red.isNull() && isRed(red), QStringLiteral("sync decode picks the frame at 0.5 s: %1").arg(error), err);
    ok &= require(red.size() == QSize(320, 180),
                  QStringLiteral("frames keep the whole picture and are never upscaled"), err);
    const QImage blue = CoverPvFrameSource::decodeFrame(pvPath, 1.5, 64, &error);
    ok &= require(!blue.isNull() && isBlue(blue) && blue.size() == QSize(112, 64),
                  QStringLiteral("sync decode seeks and scales the shorter side down to the requested side"), err);
    ok &= require(CoverPvFrameSource::decodeFrame(tempDir.filePath(QStringLiteral("missing.mp4")), 0.0, 64, &error).isNull()
                      && !error.isEmpty(),
                  QStringLiteral("a missing PV fails with an error"), err);

    CoverPvFrameSource source;
    QList<QPair<QString, double>> ready;
    QImage lastImage;
    QObject::connect(&source, &CoverPvFrameSource::frameReady,
                     [&](const QString& key, double seconds, const QImage& image) {
        ready.append({key, seconds});
        lastImage = image;
    });

    // Scrubbing: one decode runs, the newest of the later requests replaces
    // the queued one, and only the newest result is delivered.
    source.request(QStringLiteral("frame"), pvPath, 0.5, 128);
    source.request(QStringLiteral("frame"), pvPath, 1.2, 128);
    source.request(QStringLiteral("frame"), pvPath, 1.6, 128);
    ok &= require(waitFor([&] { return !ready.isEmpty(); }, 30000),
                  QStringLiteral("async decode delivers a frame"), err);
    waitFor([] { return false; }, 300);
    ok &= require(ready.size() == 1 && qAbs(ready.constFirst().second - 1.6) < 1e-9 && isBlue(lastImage),
                  QStringLiteral("latest-wins delivers only the newest request"), err);

    // Re-requesting a decoded frame is served from the cache synchronously.
    source.request(QStringLiteral("frame"), pvPath, 1.6, 128);
    ok &= require(ready.size() == 2, QStringLiteral("a cached frame is delivered without a decode"), err);

    // A cancelled key never reports back.
    source.request(QStringLiteral("other"), pvPath, 0.25, 96);
    source.cancel(QStringLiteral("other"));
    waitFor([] { return false; }, 1500);
    ok &= require(ready.size() == 2, QStringLiteral("a cancelled request is dropped"), err);

    if (ok) {
        out << "cover_pv_frame_source_spec ok" << Qt::endl;
    }
    return ok ? 0 : 1;
}
