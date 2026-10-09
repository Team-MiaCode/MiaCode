#include "export/cover_export/CoverPvFrameSource.h"

#include "export/video_export/VideoExportController.h"

#include <QFileInfo>
#include <QProcess>

namespace miacode::cover_export {

namespace {

constexpr int kDecodeTimeoutMs = 15000;
// Square frames at the export cap stay small enough (≤ 4096² ARGB) that a few
// of them cover scrubbing back and forth over one layer.
constexpr int kCacheCapacity = 4;

QImage imageFromBmp(const QByteArray& bytes)
{
    QImage image;
    if (!bytes.isEmpty()) {
        image.loadFromData(bytes, "BMP");
    }
    return image.isNull() ? QImage() : image.convertToFormat(QImage::Format_RGB32);
}

}  // namespace

CoverPvFrameSource::CoverPvFrameSource(QObject* parent)
    : QObject(parent)
{
}

CoverPvFrameSource::~CoverPvFrameSource()
{
    cancelAll();
}

QStringList CoverPvFrameSource::decodeArguments(const Request& request)
{
    const int side = qBound(16, request.sidePx, 4096);
    return {
        QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-ss"), QString::number(qMax(0.0, request.seconds), 'f', 6),
        QStringLiteral("-i"), request.mediaPath,
        QStringLiteral("-frames:v"), QStringLiteral("1"),
        QStringLiteral("-an"),
        // The whole frame, shorter side scaled down to `side` (never up): the
        // composer fills or fits it into the playfield disk itself.
        QStringLiteral("-vf"),
        QStringLiteral("scale='2*trunc(iw*min(1,%1/min(iw,ih))/2)':'2*trunc(ih*min(1,%1/min(iw,ih))/2)'"
                       ":flags=bicubic").arg(side),
        QStringLiteral("-f"), QStringLiteral("image2pipe"),
        QStringLiteral("-c:v"), QStringLiteral("bmp"),
        QStringLiteral("-"),
    };
}

QString CoverPvFrameSource::cacheKey(const Request& request)
{
    return QStringLiteral("%1|%2|%3")
        .arg(request.mediaPath)
        .arg(qRound64(request.seconds * 1000.0))
        .arg(request.sidePx);
}

QImage CoverPvFrameSource::decodeFrame(const QString& mediaPath, double seconds, int sidePx,
                                        QString* errorMessage)
{
    const auto fail = [errorMessage](const QString& message) {
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        return QImage();
    };
    if (!QFileInfo::exists(mediaPath)) {
        return fail(QStringLiteral("PV file not found: %1").arg(mediaPath));
    }
    const QString ffmpegPath = VideoExportController::ffmpegExecutablePath();
    if (ffmpegPath.isEmpty()) {
        return fail(QStringLiteral("ffmpeg executable was not found"));
    }
    QProcess process;
    process.start(ffmpegPath, decodeArguments({mediaPath, seconds, sidePx}), QIODevice::ReadOnly);
    if (!process.waitForStarted(kDecodeTimeoutMs) || !process.waitForFinished(kDecodeTimeoutMs)) {
        process.kill();
        process.waitForFinished(1000);
        return fail(QStringLiteral("ffmpeg PV frame decode timed out"));
    }
    const QImage image = imageFromBmp(process.readAllStandardOutput());
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0 || image.isNull()) {
        return fail(QStringLiteral("ffmpeg PV frame decode failed: %1")
                        .arg(QString::fromUtf8(process.readAllStandardError()).trimmed()));
    }
    return image;
}

void CoverPvFrameSource::request(const QString& key, const QString& mediaPath, double seconds, int sidePx)
{
    const Request next{mediaPath, seconds, sidePx};
    const auto cached = cache_.constFind(cacheKey(next));
    if (cached != cache_.cend()) {
        // A cached hit supersedes whatever is still decoding for this key.
        cancel(key);
        emit frameReady(key, seconds, cached.value());
        return;
    }
    Slot& slot = slots_[key];
    if (slot.process != nullptr) {
        slot.pending = next;
        slot.hasPending = true;
        return;
    }
    start(key, next);
}

void CoverPvFrameSource::start(const QString& key, const Request& request)
{
    Slot& slot = slots_[key];
    slot.running = request;
    slot.hasPending = false;
    const QString ffmpegPath = VideoExportController::ffmpegExecutablePath();
    if (ffmpegPath.isEmpty() || !QFileInfo::exists(request.mediaPath)) {
        emit frameFailed(key, ffmpegPath.isEmpty()
                                  ? QStringLiteral("ffmpeg executable was not found")
                                  : QStringLiteral("PV file not found: %1").arg(request.mediaPath));
        return;
    }
    auto* process = new QProcess(this);
    slot.process = process;
    connect(process, &QProcess::finished, this, [this, key, process]() { onFinished(key, process); });
    connect(process, &QProcess::errorOccurred, this, [this, key, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            onFinished(key, process);
        }
    });
    process->start(ffmpegPath, decodeArguments(request), QIODevice::ReadOnly);
}

void CoverPvFrameSource::onFinished(const QString& key, QProcess* process)
{
    auto it = slots_.find(key);
    if (it == slots_.end() || it->process != process) {
        process->deleteLater();
        return;
    }
    const Request finished = it->running;
    const QImage image = process->exitStatus() == QProcess::NormalExit && process->exitCode() == 0
        ? imageFromBmp(process->readAllStandardOutput())
        : QImage();
    const QString error = QString::fromUtf8(process->readAllStandardError()).trimmed();
    it->process = nullptr;
    process->deleteLater();

    if (!image.isNull()) {
        remember(cacheKey(finished), image);
    }
    if (it->hasPending) {
        // A newer request arrived while this one decoded; its result wins.
        const Request pending = it->pending;
        start(key, pending);
        return;
    }
    if (image.isNull()) {
        emit frameFailed(key, error.isEmpty() ? QStringLiteral("ffmpeg PV frame decode failed") : error);
        return;
    }
    emit frameReady(key, finished.seconds, image);
}

void CoverPvFrameSource::cancel(const QString& key)
{
    auto it = slots_.find(key);
    if (it == slots_.end()) {
        return;
    }
    QProcess* process = it->process;
    slots_.erase(it);
    if (process != nullptr) {
        // Detach first so neither the kill nor a teardown-time destruction can
        // call back into this object.
        process->disconnect(this);
        process->kill();
        process->waitForFinished(1000);
        process->deleteLater();
    }
}

void CoverPvFrameSource::cancelAll()
{
    const QStringList keys = slots_.keys();
    for (const QString& key : keys) {
        cancel(key);
    }
}

void CoverPvFrameSource::remember(const QString& key, const QImage& image)
{
    if (!cache_.contains(key)) {
        cacheOrder_.append(key);
    }
    cache_.insert(key, image);
    while (cacheOrder_.size() > kCacheCapacity) {
        cache_.remove(cacheOrder_.takeFirst());
    }
}

}  // namespace miacode::cover_export
