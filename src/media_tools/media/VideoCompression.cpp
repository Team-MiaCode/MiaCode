#include "media_tools/media/VideoCompression.h"

#include "media_tools/media/PvCompressionPolicy.h"
#include "common/DebugLog.h"
#include "common/DebugOptions.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QThread>

#include <cmath>
#include <filesystem>
#include <system_error>

namespace miacode::media {
namespace {

constexpr int kProcessPollMs = 100;
constexpr int kFileRetryAttempts = 40;
constexpr int kFileRetryDelayMs = 50;

class CompressionContext {
public:
    CompressionContext(
        const VideoCompressionCancellation& cancellation,
        const VideoCompressionProgress& progress)
        : cancellation_(cancellation), progress_(progress)
    {}

    bool cancelled() const
    {
        return cancellation_ && cancellation_->load(std::memory_order_relaxed);
    }

    VideoCompressionStage stage() const { return stage_; }

    void report(VideoCompressionStage stage, int percent)
    {
        if (stage == stage_ && percent == percent_) {
            return;
        }
        stage_ = stage;
        percent_ = percent;
        if (progress_) {
            progress_(stage, percent);
        }
    }

private:
    const VideoCompressionCancellation& cancellation_;
    const VideoCompressionProgress& progress_;
    VideoCompressionStage stage_ = VideoCompressionStage::Backup;
    int percent_ = -2;
};

void stopProcess(QProcess& process)
{
    if (process.state() != QProcess::NotRunning) {
        process.kill();
        process.waitForFinished(2000);
    }
}

bool startProcess(
    QProcess& process,
    const QString& ffmpegPath,
    const QStringList& arguments,
    const CompressionContext& context,
    LocalizedText* error)
{
    if (context.cancelled()) {
        return false;
    }
    process.start(ffmpegPath, arguments, QIODevice::ReadOnly);
    QElapsedTimer timer;
    timer.start();
    while (!context.cancelled()) {
        if (process.waitForStarted(kProcessPollMs)) {
            return true;
        }
        if (process.state() == QProcess::NotRunning || timer.hasExpired(5000)) {
            break;
        }
    }
    stopProcess(process);
    if (!context.cancelled()) {
        *error = process.errorString();
    }
    return false;
}

void appendDiagnosticTail(QString& tail, const QByteArray& bytes)
{
    tail += QString::fromLocal8Bit(bytes);
    if (tail.size() > 8000) {
        tail = tail.right(8000);
    }
}

std::filesystem::path nativePath(const QString& path)
{
#ifdef Q_OS_WIN
    return std::filesystem::path(path.toStdWString());
#else
    return std::filesystem::path(QFile::encodeName(path).toStdString());
#endif
}

bool renameWithRetry(const QString& sourcePath, const QString& destinationPath)
{
    const std::filesystem::path source = nativePath(sourcePath);
    const std::filesystem::path destination = nativePath(destinationPath);
    std::error_code error;
    int attempts = 0;
    for (int attempt = 0; attempt < kFileRetryAttempts; ++attempt) {
        ++attempts;
        if (QFileInfo::exists(destinationPath)) {
            error = std::make_error_code(std::errc::file_exists);
            break;
        }
        // Same-directory renames must never fall back to copying a whole video.
        std::filesystem::rename(source, destination, error);
        if (!error) {
            return true;
        }
        QThread::msleep(kFileRetryDelayMs);
    }
    debug_log::appendLine(
        debug_log::Channel::Runtime, QStringLiteral("media/video_compression"),
        QStringLiteral("rename_failed source=%1 destination=%2 attempts=%3 error_category=%4 error_code=%5 error=%6")
            .arg(sourcePath, destinationPath).arg(attempts)
            .arg(QString::fromLatin1(error.category().name())).arg(error.value())
            .arg(QString::fromLocal8Bit(error.message().c_str())),
        /*force=*/true, debug_log::Level::Error);
    return false;
}

void preserveRecoveryFiles(QTemporaryDir& temporaryDirectory, LocalizedText* error)
{
    temporaryDirectory.setAutoRemove(false);
    *error = *error + QStringLiteral("\n")
        + localizedText("media_tools.recovery_files_1").arg(temporaryDirectory.path());
}

bool copyBackup(
    const QString& videoPath,
    const QString& temporaryPath,
    CompressionContext& context,
    LocalizedText* error)
{
    context.report(VideoCompressionStage::Backup, 0);
    if (context.cancelled()) {
        return false;
    }
    QFile input(videoPath);
    QFile output(temporaryPath);
    if (!input.open(QIODevice::ReadOnly)) {
        *error = input.errorString();
        return false;
    }
    if (!output.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
        *error = localizedText("media_tools.failed_to_write_file_1").arg(temporaryPath);
        return false;
    }
    const qint64 totalBytes = input.size();
    qint64 copiedBytes = 0;
    while (!input.atEnd()) {
        if (context.cancelled()) {
            return false;
        }
        const QByteArray bytes = input.read(1024 * 1024);
        if (bytes.isEmpty()) {
            *error = input.errorString();
            return false;
        }
        qint64 writtenBytes = 0;
        while (writtenBytes < bytes.size()) {
            if (context.cancelled()) {
                return false;
            }
            const qint64 written = output.write(
                bytes.constData() + writtenBytes, bytes.size() - writtenBytes);
            if (written <= 0) {
                *error = output.errorString();
                return false;
            }
            writtenBytes += written;
        }
        copiedBytes += bytes.size();
        if (totalBytes > 0) {
            context.report(VideoCompressionStage::Backup,
                qBound(0, static_cast<int>(static_cast<double>(copiedBytes) / totalBytes * 99.0), 99));
        }
    }
    if (!output.flush()) {
        *error = output.errorString();
        return false;
    }
    return !context.cancelled();
}

bool publishBackup(
    const QString& temporaryBackupPath,
    const QString& backupPath,
    QTemporaryDir& temporaryDirectory,
    CompressionContext& context,
    LocalizedText* error)
{
    if (context.cancelled()) {
        return false;
    }
    const bool hadBackup = QFileInfo::exists(backupPath);
    const QString previousBackupPath = temporaryDirectory.filePath(QStringLiteral("previous_backup.mp4"));
    if (hadBackup && (!QFileInfo(backupPath).isFile()
        || !renameWithRetry(backupPath, previousBackupPath))) {
        *error = localizedText("media_tools.failed_to_write_file_1").arg(backupPath);
        return false;
    }
    // Once replacement starts, finish or restore the previous backup as a unit.
    if (!renameWithRetry(temporaryBackupPath, backupPath)) {
        *error = localizedText("media_tools.failed_to_write_file_1").arg(backupPath);
        if (hadBackup && !renameWithRetry(previousBackupPath, backupPath)) {
            preserveRecoveryFiles(temporaryDirectory, error);
        }
        return false;
    }
    context.report(VideoCompressionStage::Backup, 100);
    return true;
}

bool probeDuration(
    const QString& ffmpegPath,
    const QString& videoPath,
    double* durationSeconds,
    CompressionContext& context,
    LocalizedText* error)
{
    context.report(VideoCompressionStage::Probe, -1);
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    if (!startProcess(process, ffmpegPath, {
            QStringLiteral("-hide_banner"), QStringLiteral("-nostdin"),
            QStringLiteral("-i"), videoPath
        }, context, error)) {
        return false;
    }
    QElapsedTimer timer;
    timer.start();
    QString output;
    while (process.state() != QProcess::NotRunning) {
        process.waitForReadyRead(kProcessPollMs);
        appendDiagnosticTail(output, process.readAll());
        if (context.cancelled()) {
            stopProcess(process);
            return false;
        }
        if (timer.hasExpired(30000)) {
            stopProcess(process);
            *error = localizedText("media_tools.duration_probe_timeout");
            return false;
        }
    }
    appendDiagnosticTail(output, process.readAll());
    if (context.cancelled()) {
        return false;
    }
    static const QRegularExpression pattern(
        QStringLiteral(R"(Duration:\s*(\d+):(\d+):(\d+(?:\.\d+)?))"));
    const QRegularExpressionMatch match = pattern.match(output);
    // A header-only ffmpeg request exits with code 1; validate the parsed header.
    if (process.exitStatus() != QProcess::NormalExit || !match.hasMatch()) {
        *error = output.trimmed().isEmpty()
            ? localizedText("media_tools.duration_read_failed")
            : LocalizedText(output.trimmed().right(2000));
        return false;
    }
    const double total = match.captured(1).toDouble() * 3600.0
        + match.captured(2).toDouble() * 60.0 + match.captured(3).toDouble();
    if (!(total > 0.0) || !std::isfinite(total)) {
        *error = localizedText("media_tools.duration_invalid");
        return false;
    }
    *durationSeconds = total;
    return true;
}

bool runCompressionPass(
    const QString& ffmpegPath,
    const QStringList& arguments,
    double durationSeconds,
    int passNumber,
    CompressionContext& context,
    LocalizedText* error)
{
    const int progressOffset = passNumber == 1 ? 0 : 50;
    context.report(VideoCompressionStage::Encode, progressOffset);
    QStringList progressArguments{
        QStringLiteral("-progress"), QStringLiteral("pipe:1"), QStringLiteral("-nostats")};
    progressArguments << arguments;
    QProcess process;
    process.setProcessChannelMode(QProcess::SeparateChannels);
    if (!startProcess(process, ffmpegPath, progressArguments, context, error)) {
        return false;
    }
    QString progressBuffer;
    QString stderrTail;
    const auto pump = [&]() {
        progressBuffer += QString::fromLatin1(process.readAllStandardOutput());
        const int lastNewline = progressBuffer.lastIndexOf(QLatin1Char('\n'));
        if (lastNewline >= 0) {
            const QString complete = progressBuffer.left(lastNewline);
            progressBuffer = progressBuffer.mid(lastNewline + 1);
            static const QRegularExpression outTimePattern(QStringLiteral(R"(out_time_us=(\d+))"));
            QRegularExpressionMatchIterator matches = outTimePattern.globalMatch(complete);
            qint64 lastMicros = -1;
            while (matches.hasNext()) {
                lastMicros = matches.next().captured(1).toLongLong();
            }
            if (lastMicros >= 0) {
                const double fraction = static_cast<double>(lastMicros) / 1000000.0 / durationSeconds;
                context.report(VideoCompressionStage::Encode,
                    progressOffset + qBound(0, qRound(fraction * 50.0), 49));
            }
        }
        if (progressBuffer.size() > 8000) {
            progressBuffer = progressBuffer.right(8000);
        }
        appendDiagnosticTail(stderrTail, process.readAllStandardError());
    };
    while (process.state() != QProcess::NotRunning) {
        process.waitForReadyRead(kProcessPollMs);
        pump();
        if (context.cancelled()) {
            stopProcess(process);
            return false;
        }
    }
    pump();
    if (context.cancelled()) {
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        *error = stderrTail.trimmed().isEmpty()
            ? localizedText("media_tools.ffmpeg_exit_error").arg(process.exitCode())
            : LocalizedText(stderrTail.trimmed().right(2000));
        return false;
    }
    return true;
}

VideoCompressionStatus preserveCompressedVideo(
    const QString& compressedPath,
    const QString& videoPath,
    QTemporaryDir& temporaryDirectory,
    LocalizedText* error)
{
    const QFileInfo videoInfo(videoPath);
    const QString preservedPath = videoInfo.dir().filePath(
        QStringLiteral("%1_compressed.%2").arg(videoInfo.completeBaseName(), videoInfo.suffix()));
    for (int attempt = 0; attempt < kFileRetryAttempts && QFileInfo::exists(preservedPath); ++attempt) {
        if (QFile::remove(preservedPath)) {
            break;
        }
        QThread::msleep(kFileRetryDelayMs);
    }
    if (renameWithRetry(compressedPath, preservedPath)) {
        const LocalizedText detail = *error;
        *error = localizedText("media_tools.compressed_but_replace_failed")
            .arg(QDir::toNativeSeparators(preservedPath));
        if (!detail.isEmpty()) {
            *error = *error + QStringLiteral("\n\n") + detail;
        }
        return VideoCompressionStatus::PreservedCompressed;
    }
    preserveRecoveryFiles(temporaryDirectory, error);
    return VideoCompressionStatus::Failed;
}

VideoCompressionStatus commitCompressedVideo(
    const QString& videoPath,
    const QString& compressedPath,
    QTemporaryDir& temporaryDirectory,
    CompressionContext& context,
    LocalizedText* error)
{
    context.report(VideoCompressionStage::Finalize, 99);
    if (context.cancelled()) {
        return VideoCompressionStatus::Cancelled;
    }
    const QString originalPath = temporaryDirectory.filePath(QStringLiteral("original.mp4"));
    // Ignore cancellation inside this short transaction so it always commits or rolls back.
    if (!renameWithRetry(videoPath, originalPath)) {
        *error = localizedText("media_tools.failed_to_stage_original_file").arg(videoPath);
        return preserveCompressedVideo(compressedPath, videoPath, temporaryDirectory, error);
    }
    if (!renameWithRetry(compressedPath, videoPath)) {
        *error = localizedText("media_tools.replace_failed").arg(videoPath);
        if (!renameWithRetry(originalPath, videoPath)) {
            preserveRecoveryFiles(temporaryDirectory, error);
            return VideoCompressionStatus::Failed;
        }
        return preserveCompressedVideo(compressedPath, videoPath, temporaryDirectory, error);
    }
    context.report(VideoCompressionStage::Finalize, 100);
    return VideoCompressionStatus::Compressed;
}

} // namespace

VideoCompressionResult runVideoCompression(
    const VideoCompressionRequest& request,
    const VideoCompressionCancellation& cancellation,
    const VideoCompressionProgress& progress)
{
    CompressionContext context(cancellation, progress);
    VideoCompressionResult result;
    const auto failedResult = [&]() {
        result.stage = context.stage();
        if (context.cancelled() && result.error.isEmpty()) {
            result.status = VideoCompressionStatus::Cancelled;
        } else {
            debug_log::appendLine(
                debug_log::Channel::Runtime, QStringLiteral("media/video_compression"),
                QStringLiteral("failed file=%1 stage=%2 error=%3")
                    .arg(request.videoPath).arg(static_cast<int>(result.stage)).arg(result.error.text()),
                /*force=*/true, debug_log::Level::Error);
        }
        return result;
    };
    if (context.cancelled()) {
        return failedResult();
    }
    const QFileInfo videoInfo(request.videoPath);
    const qint64 originalBytes = videoInfo.size();
    if (!videoInfo.isFile() || originalBytes <= 0) {
        result.error = localizedText("media_tools.batch_pv_invalid_file");
        return failedResult();
    }
    if (originalBytes < kPvCompressionHardLimitBytes) {
        result.error = localizedText("media_tools.the_current_video_is_already");
        return failedResult();
    }
    QTemporaryDir temporaryDirectory(videoInfo.dir().filePath(QStringLiteral(".miacode_video_compress_XXXXXX")));
    if (!temporaryDirectory.isValid()) {
        result.error = localizedText("media_tools.pass_log_directory_failed");
        return failedResult();
    }
    const QString temporaryBackupPath = temporaryDirectory.filePath(QStringLiteral("backup.mp4"));
    const QString backupPath = videoInfo.dir().filePath(
        QStringLiteral("%1_bak.%2").arg(videoInfo.completeBaseName(), videoInfo.suffix()));
    if (!copyBackup(request.videoPath, temporaryBackupPath, context, &result.error)
        || !publishBackup(temporaryBackupPath, backupPath, temporaryDirectory, context, &result.error)) {
        return failedResult();
    }
    double durationSeconds = 0.0;
    if (!probeDuration(request.ffmpegPath, backupPath, &durationSeconds, context, &result.error)) {
        return failedResult();
    }
    const QString compressedPath = temporaryDirectory.filePath(QStringLiteral("compressed.mp4"));
    PvCompressionPlan plan = makePvCompressionPlan(durationSeconds);
    qint64 compressedBytes = 0;
    bool encoded = false;
    for (int attempt = 0; attempt < 2; ++attempt) {
        const QString passLogPath = temporaryDirectory.filePath(QStringLiteral("x264-attempt-%1").arg(attempt));
        for (int passNumber = 1; passNumber <= 2; ++passNumber) {
            const QStringList arguments = makePvCompressionPassArguments(
                backupPath, compressedPath, passLogPath, plan, passNumber);
            if (!runCompressionPass(request.ffmpegPath, arguments, durationSeconds,
                    passNumber, context, &result.error)) {
                return failedResult();
            }
        }
        compressedBytes = QFileInfo(compressedPath).size();
        if (isAcceptablePvCompressionOutput(originalBytes, compressedBytes)) {
            encoded = true;
            break;
        }
        if (attempt == 0 && compressedBytes >= kPvCompressionHardLimitBytes) {
            plan = adjustedPvCompressionPlan(plan, compressedBytes);
        } else {
            break;
        }
    }
    if (!encoded) {
        result.error = compressedBytes >= kPvCompressionHardLimitBytes
            ? localizedText("media_tools.compressed_size_exceeded")
            : localizedText("media_tools.compressed_size_not_reduced");
        return failedResult();
    }
    result.status = commitCompressedVideo(
        request.videoPath, compressedPath, temporaryDirectory, context, &result.error);
    result.stage = context.stage();
    if (result.status == VideoCompressionStatus::Compressed) {
        result.error.clear();
        if (debug_options::runtimeDebugOutputEnabled()) {
            debug_log::appendLine(
                debug_log::Channel::Runtime, QStringLiteral("media/video_compression"),
                QStringLiteral("compressed file=%1 original_bytes=%2 compressed_bytes=%3 duration_seconds=%4")
                    .arg(request.videoPath).arg(originalBytes).arg(compressedBytes).arg(durationSeconds, 0, 'f', 3));
        }
    }
    return result;
}

} // namespace miacode::media
