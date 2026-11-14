#include "app/ui/export/AnimatedImageClipboardService.h"

#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLockFile>
#include <QMimeData>
#include <QSet>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>

namespace miacode::ui {
namespace {

const QString kGifMime = QStringLiteral("image/gif");
#ifdef Q_OS_WIN
const QString kWindowsGifMime = QStringLiteral("application/x-qt-windows-mime;value=\"GIF\"");
#endif

QSet<QString> clipboardFilePaths(QClipboard* clipboard)
{
    QSet<QString> paths;
    const QMimeData* data = clipboard != nullptr ? clipboard->mimeData() : nullptr;
    if (data != nullptr) {
        for (const QUrl& url : data->urls()) {
            if (url.isLocalFile()) paths.insert(QDir::cleanPath(url.toLocalFile()));
        }
    }
    return paths;
}

} // namespace

AnimatedImageClipboardService::AnimatedImageClipboardService(QObject* parent)
    : QObject(parent)
    , clipboard_(QGuiApplication::clipboard())
    , cacheDirectory_(QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))
          .filePath(QStringLiteral("clipboard-gif")))
{
    pruneCache();
}

AnimatedImageClipboardService::~AnimatedImageClipboardService()
{
    discardGif(pendingPath_);
}

QString AnimatedImageClipboardService::reserveGifPath(QString* errorMessage)
{
    if (!pendingPath_.isEmpty()) {
        if (errorMessage) *errorMessage = qtTrId("job_progress.busy");
        return {};
    }
    if (clipboard_ == nullptr || !QDir().mkpath(cacheDirectory_)) {
        if (errorMessage) *errorMessage = qtTrId("export.gif.clipboard_unavailable");
        return {};
    }
    pruneCache();
    const QString path = QDir(cacheDirectory_).filePath(
        QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral(".gif"));
    auto lock = std::make_unique<QLockFile>(path + QStringLiteral(".lock"));
    lock->setStaleLockTime(0);
    if (!lock->tryLock(0)) {
        if (errorMessage) *errorMessage = qtTrId("export.gif.clipboard_unavailable");
        return {};
    }
    pendingPath_ = path;
    pendingFileLock_ = std::move(lock);
    return path;
}

bool AnimatedImageClipboardService::publishGif(const QString& path, QString* errorMessage)
{
    QFile file(path);
    if (clipboard_ == nullptr || path != pendingPath_ || pendingFileLock_ == nullptr
        || !file.open(QIODevice::ReadOnly)) {
        if (errorMessage) *errorMessage = qtTrId("export.gif.clipboard_failed");
        return false;
    }
    const QByteArray gifBytes = file.readAll();
    const bool readSucceeded = file.error() == QFileDevice::NoError
        && gifBytes.size() == file.size();
    file.close();
    if (!readSucceeded || (!gifBytes.startsWith("GIF89a") && !gifBytes.startsWith("GIF87a"))) {
        if (errorMessage) *errorMessage = qtTrId("export.gif.clipboard_failed");
        return false;
    }
    // Receivers can choose animation bytes or the cached file attachment.
    auto* data = new QMimeData;
    data->setData(kGifMime, gifBytes);
#ifdef Q_OS_WIN
    data->setData(kWindowsGifMime, gifBytes);
#endif
    data->setUrls({QUrl::fromLocalFile(path)});
    clipboard_->setMimeData(data);
    // Clipboard ownership and QMimeData identity are platform-specific.
    const QMimeData* published = clipboard_->mimeData();
    const bool copied = published != nullptr
        && published->data(kGifMime) == gifBytes
        && published->urls().contains(QUrl::fromLocalFile(path));
    if (!copied) {
        if (errorMessage) *errorMessage = qtTrId("export.gif.clipboard_failed");
        return false;
    }
    pendingPath_.clear();
    pendingFileLock_.reset();
    return true;
}

void AnimatedImageClipboardService::discardGif(const QString& path)
{
    if (pendingPath_.isEmpty() || path != pendingPath_) return;
    if (!clipboardFilePaths(clipboard_).contains(path)) QFile::remove(path);
    pendingPath_.clear();
    pendingFileLock_.reset();
}

void AnimatedImageClipboardService::pruneCache()
{
    if (clipboard_ == nullptr) return;
    const QSet<QString> referencedPaths = clipboardFilePaths(clipboard_);
    const QDateTime cutoff = QDateTime::currentDateTimeUtc().addDays(-7);
    const QDir directory(cacheDirectory_);
    for (const QFileInfo& file : directory.entryInfoList({QStringLiteral("*.gif")}, QDir::Files)) {
        const QString path = file.absoluteFilePath();
        if (file.lastModified() >= cutoff || path == pendingPath_ || referencedPaths.contains(path)) continue;
        // Other application instances can be rendering into the same cache.
        QLockFile lock(path + QStringLiteral(".lock"));
        lock.setStaleLockTime(0);
        if (lock.tryLock(0)) QFile::remove(path);
    }
}

} // namespace miacode::ui
