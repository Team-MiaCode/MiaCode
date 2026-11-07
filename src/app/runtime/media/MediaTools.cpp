#include "common/LocalizedText.h"

#include "media_tools/media/PvCompressionPolicy.h"
#include "media_tools/media/MediaPrependPolicy.h"
#include "app/services/UiRequestService.h"
#include "app/services/JobProgressService.h"
#include "app/services/ChartWorkspace.h"
#include "app/runtime/media/MediaJobsHost.h"
#include "app/runtime/Shared.h"
#include "app/runtime/shell/ShellHost.h"

#include "AppVersion.h"
#include "audio/QtPreviewSfxRuntime.h"
#include "core/chart/ChartAssetPaths.h"
#include "core/chart/document/ChartClockCount.h"
#include "app/services/Id3TagReader.h"
#include "common/OperationLog.h"
#include "core/scene/PreviewSfxAssets.h"
#include "core/video/PreviewGameplayConfig.h"
#include "audio/WaveformCache.h"
#include "preview/runtime/PreviewRuntime.h"
#include "app/runtime/latency/LatencyAnalysis.h"

#include <QDesktopServices>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QUrl>
#include <QtCore>
#include <QtGui>
#include <cstring>
#include <cmath>

#include "common/DebugLog.h"

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <RestartManager.h>
#pragma comment(lib, "Rstrtmgr.lib")
#endif

using namespace miacode::runtime::shared;

namespace {

// --- "pv占用" diagnostics -------------------------------------------------
// Name the process(es) currently holding `path` open, via the Windows Restart
// Manager. Tells us whether the lock is MiaCode itself (internal handle leak)
// or an external program. See project_pv_file_lock_release.
#ifdef Q_OS_WIN
QString describeFileLockHolders(const QString& path)
{
    DWORD session = 0;
    WCHAR sessionKey[CCH_RM_SESSION_KEY + 1] = {0};
    if (RmStartSession(&session, 0, sessionKey) != ERROR_SUCCESS) {
        return qtTrId("media_tools.lock_session_failed");
    }
    QString result;
    const std::wstring native = QDir::toNativeSeparators(path).toStdWString();
    LPCWSTR resources[1] = { native.c_str() };
    if (RmRegisterResources(session, 1, resources, 0, nullptr, 0, nullptr) == ERROR_SUCCESS) {
        UINT needed = 0;
        UINT count = 16;
        RM_PROCESS_INFO infos[16];
        DWORD reasons = 0;
        const DWORD rc = RmGetList(session, &needed, &count, infos, &reasons);
        if (rc == ERROR_SUCCESS || rc == ERROR_MORE_DATA) {
            const DWORD self = GetCurrentProcessId();
            const UINT shown = count < 16 ? count : 16;
            QStringList holders;
            for (UINT i = 0; i < shown; ++i) {
                const DWORD pid = infos[i].Process.dwProcessId;
                QString name = QString::fromWCharArray(infos[i].strAppName);
                if (name.isEmpty()) {
                    name = QStringLiteral("?");
                }
                holders << QStringLiteral("%1(PID=%2%3)")
                               .arg(name)
                               .arg(pid)
                               .arg(pid == self ? qtTrId("media_tools.lock_holder_this_process") : QString());
            }
            if (needed > shown) {
                holders << QStringLiteral("…(+%1)").arg(needed - shown);
            }
            result = holders.isEmpty() ? qtTrId("media_tools.lock_holder_none") : holders.join(QStringLiteral("; "));
        } else {
            result = qtTrId("media_tools.lock_query_failed").arg(static_cast<uint>(rc));
        }
    } else {
        result = qtTrId("media_tools.lock_register_failed");
    }
    RmEndSession(session);
    return result;
}
// Try to open with NO sharing. Succeeds only if nothing holds the file; on
// failure the Win32 error distinguishes a sharing lock (32) from access-denied
// (5), not-found (2), etc. — so we can tell "locked" from "fails for another
// reason" even when the Restart Manager reports no holder.
QString probeWin32Access(const QString& path)
{
    const std::wstring native = QDir::toNativeSeparators(path).toStdWString();
    HANDLE h = CreateFileW(native.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        CloseHandle(h);
        return QStringLiteral("excl_open=ok");
    }
    return QStringLiteral("excl_open=err%1").arg(static_cast<uint>(GetLastError()));
}
#else
QString describeFileLockHolders(const QString&) { return QString(); }
QString probeWin32Access(const QString&) { return QString(); }
#endif

void logFileLockDiag(const QString& where, const QString& path)
{
    miacode::debug_log::appendLine(
        miacode::debug_log::Channel::Fatal,
        QStringLiteral("diag/file_lock"),
        QStringLiteral("where=%1 path=%2 %3 holders=%4")
            .arg(where, path, probeWin32Access(path), describeFileLockHolders(path)));
}

bool mediaToolFileIsExecutable(const QString& path)
{
    const QFileInfo info(path);
    return info.exists() && info.isFile() && info.isExecutable();
}

QString resolveMediaToolFfmpegExecutable()
{
#ifdef Q_OS_WIN
    const QString ffmpegName = QStringLiteral("ffmpeg.exe");
#else
    const QString ffmpegName = QStringLiteral("ffmpeg");
#endif
    const QString envPath = qEnvironmentVariable("MIACODE_FFMPEG_PATH", qEnvironmentVariable("MIACODE_FFMPEG"));
    if (mediaToolFileIsExecutable(envPath)) {
        return QDir::cleanPath(QFileInfo(envPath).absoluteFilePath());
    }

    const QDir appDir(QCoreApplication::applicationDirPath());
    const QStringList candidates{
        appDir.filePath(ffmpegName),
        appDir.filePath(QStringLiteral("ffmpeg/%1").arg(ffmpegName)),
        appDir.filePath(QStringLiteral("../Resources/%1").arg(ffmpegName)),
        appDir.filePath(QStringLiteral("../../third_party/ffmpeg/windows/%1").arg(ffmpegName)),
        appDir.filePath(QStringLiteral("../../third_party/ffmpeg/macos/%1").arg(ffmpegName)),
        appDir.filePath(QStringLiteral("../../third_party/ffmpeg/linux/%1").arg(ffmpegName)),
        appDir.filePath(QStringLiteral("../../../third_party/ffmpeg/windows/%1").arg(ffmpegName)),
        appDir.filePath(QStringLiteral("../../../third_party/ffmpeg/macos/%1").arg(ffmpegName)),
        appDir.filePath(QStringLiteral("../../../third_party/ffmpeg/linux/%1").arg(ffmpegName)),
        QStringLiteral("/opt/homebrew/bin/%1").arg(ffmpegName),
        QStringLiteral("/usr/local/bin/%1").arg(ffmpegName),
    };
    for (const QString& candidate : candidates) {
        if (mediaToolFileIsExecutable(candidate)) {
            return QDir::cleanPath(QFileInfo(candidate).absoluteFilePath());
        }
    }

    const QString fromPath = QStandardPaths::findExecutable(ffmpegName);
    if (mediaToolFileIsExecutable(fromPath)) {
        return QDir::cleanPath(QFileInfo(fromPath).absoluteFilePath());
    }
    return QString();
}

// Windows can briefly refuse a rename/remove while the file is still held by a
// just-released decoder handle, an antivirus on-write scan, or Explorer's
// preview/thumbnail handler. Retry with a short backoff (~2s max) so these
// transient locks don't fail the whole operation. processEvents keeps the modal
// progress UI responsive between attempts. See project_pv_file_lock_release.
constexpr int kFileLockRetryAttempts = 40;
constexpr int kFileLockRetryDelayMs = 50;

void pumpFileLockRetryDelay()
{
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    QThread::msleep(kFileLockRetryDelayMs);
}

bool removeWithRetry(const QString& path)
{
    if (!QFileInfo::exists(path)) {
        return true;
    }
    for (int attempt = 0; attempt < kFileLockRetryAttempts; ++attempt) {
        if (QFile::remove(path)) {
            return true;
        }
        pumpFileLockRetryDelay();
    }
    return false;
}

bool renameWithRetry(const QString& from, const QString& to)
{
    for (int attempt = 0; attempt < kFileLockRetryAttempts; ++attempt) {
        if (QFile::rename(from, to)) {
            return true;
        }
        pumpFileLockRetryDelay();
    }
    return false;
}

bool copyFileReplacing(const QString& sourcePath, const QString& destinationPath, miacode::LocalizedText* error)
{
    // Diagnostic bracket #1: who holds the source right after the preview
    // release (and before any ffmpeg work). For pv ops sourcePath is pv.mp4.
    logFileLockDiag(QStringLiteral("copy_entry_src"), sourcePath);
    removeWithRetry(destinationPath);
    for (int attempt = 0; attempt < kFileLockRetryAttempts; ++attempt) {
        if (QFile::copy(sourcePath, destinationPath)) {
            return true;
        }
        // A failed copy can leave a partial/zero-byte destination; clear it
        // before the next attempt so QFile::copy doesn't bail on "already exists".
        QFile::remove(destinationPath);
        pumpFileLockRetryDelay();
    }
    if (error != nullptr) {
        *error = miacode::localizedText("media_tools.failed_to_write_file_1")
            .arg(destinationPath);
    }
    return false;
}

bool restoreFileFromBackup(const QString& backupPath, const QString& destinationPath, miacode::LocalizedText* error)
{
    if (!QFileInfo::exists(backupPath)) {
        if (error != nullptr) {
            *error = miacode::localizedText("media_tools.backup_not_found").arg(backupPath);
        }
        return false;
    }
    if (!copyFileReplacing(backupPath, destinationPath, error)) {
        if (error != nullptr) {
            *error = miacode::localizedText("media_tools.failed_to_restore_backup_to")
                .arg(destinationPath);
        }
        return false;
    }
    return true;
}

int mediaBlankClockCountFromFields(const QVector<SimaiRawField>& fields)
{
    for (const SimaiRawField& field : fields) {
        if (field.key.compare(QStringLiteral("clock_count"), Qt::CaseInsensitive) != 0
            && field.key.compare(QStringLiteral("clockcount"), Qt::CaseInsensitive) != 0) {
            continue;
        }
        bool ok = false;
        const int value = field.value.trimmed().toInt(&ok);
        if (ok && value > 0) {
            return value;
        }
        return 0;
    }
    return 0;
}

int mediaBlankBeatsFromMeterId(const QString& meterId)
{
    bool ok = false;
    const int slash = meterId.indexOf(QChar('/'));
    const int numerator = slash > 0 ? meterId.left(slash).toInt(&ok) : 0;
    return ok && numerator > 0 ? numerator : 4;
}

bool replaceFileWithTemp(const QString& tempPath, const QString& destinationPath, miacode::LocalizedText* error)
{
    const QString replacingPath = destinationPath + QStringLiteral(".replacing");
    removeWithRetry(replacingPath);
    // Diagnostic bracket #2: who holds the file right before we rename it
    // (after ffmpeg finished). Compare with bracket #1 to tell "never released"
    // from "re-acquired during the encode".
    logFileLockDiag(QStringLiteral("before_rename"), destinationPath);
    if (!renameWithRetry(destinationPath, replacingPath)) {
        const QString diag = QStringLiteral("%1 | %2")
                                 .arg(probeWin32Access(destinationPath),
                                      describeFileLockHolders(destinationPath));
        logFileLockDiag(QStringLiteral("rename_failed"), destinationPath);
        if (error != nullptr) {
            *error = miacode::localizedText("media_tools.failed_to_stage_original_file")
                .arg(destinationPath)
                + miacode::localizedText("media_tools.lock_diagnosis").arg(diag);
        }
        return false;
    }
    if (!renameWithRetry(tempPath, destinationPath)) {
        renameWithRetry(replacingPath, destinationPath);
        if (error != nullptr) {
            *error = miacode::localizedText("media_tools.replace_failed").arg(destinationPath);
        }
        return false;
    }
    removeWithRetry(replacingPath);
    return true;
}

// Runs ffmpeg on the shell's shared progress surface. When
// `totalDurationSeconds` is positive the bar tracks real percent-done by
// reading ffmpeg's machine-readable `-progress pipe:1` stream: each block
// carries an `out_time_us=` line (the output timestamp reached so far) which,
// divided by the expected total duration, gives the percentage. When the
// duration is unknown (<= 0) it falls back to an indeterminate busy bar.
bool runFfmpegBlocking(
    const QString& ffmpegPath,
    const QStringList& args,
    miacode::JobProgressService* jobProgress,
    const miacode::LocalizedText& title,
    const miacode::LocalizedText& label,
    double totalDurationSeconds,
    miacode::LocalizedText* error,
    bool* cancelled = nullptr,
    quint64 existingJobToken = 0)
{
    if (jobProgress == nullptr) {
        if (error != nullptr) {
            *error = miacode::localizedText("media_tools.progress_unavailable");
        }
        return false;
    }
    if (cancelled != nullptr) {
        *cancelled = false;
    }
    const bool determinate = totalDurationSeconds > 0.0;
    const bool ownsJob = existingJobToken == 0;
    const quint64 jobToken = ownsJob
        ? jobProgress->begin(title, label, /*cancellable=*/true)
        : existingJobToken;
    const auto cancellationRequested = [jobProgress, jobToken, cancelled, error]() {
        if (jobProgress->active() && jobProgress->token() == jobToken
            && !jobProgress->cancelRequested()) {
            return false;
        }
        if (cancelled != nullptr) {
            *cancelled = true;
        }
        if (error != nullptr) {
            *error = miacode::localizedText("media_tools.canceled");
        }
        return true;
    };
    const auto endJob = [jobProgress, jobToken, ownsJob]() {
        if (ownsJob && jobProgress->token() == jobToken) {
            jobProgress->end();
        }
    };
    if (cancellationRequested()) {
        endJob();
        return false;
    }
    if (determinate) {
        jobProgress->report(0, label);
    } else {
        jobProgress->reportIndeterminate(label);
    }
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    if (cancellationRequested()) {
        endJob();
        return false;
    }

    QStringList progressArgs;
    progressArgs << QStringLiteral("-progress") << QStringLiteral("pipe:1")
                 << QStringLiteral("-nostats")
                 << args;

    QProcess process;
    // Keep the channels separate: `-progress` writes to stdout while ffmpeg's
    // human-readable diagnostics (which we surface on failure) go to stderr.
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start(ffmpegPath, progressArgs, QIODevice::ReadOnly);
    if (!process.waitForStarted(5000)) {
        endJob();
        if (error != nullptr) {
            *error = process.errorString();
        }
        return false;
    }

    QString progressBuffer;
    QString stderrTail;
    const auto pump = [&]() {
        const QByteArray out = process.readAllStandardOutput();
        if (jobProgress->token() == jobToken && determinate && !out.isEmpty()) {
            progressBuffer += QString::fromLatin1(out);
            const int lastNewline = progressBuffer.lastIndexOf(QLatin1Char('\n'));
            if (lastNewline >= 0) {
                const QString complete = progressBuffer.left(lastNewline);
                progressBuffer = progressBuffer.mid(lastNewline + 1);
                static const QRegularExpression outTimePattern(QStringLiteral(R"(out_time_us=(\d+))"));
                qint64 lastMicros = -1;
                QRegularExpressionMatchIterator it = outTimePattern.globalMatch(complete);
                while (it.hasNext()) {
                    lastMicros = it.next().captured(1).toLongLong();
                }
                if (lastMicros >= 0) {
                    const double seconds = static_cast<double>(lastMicros) / 1000000.0;
                    // Cap at 99% until the process actually exits so the
                    // bar doesn't read "done" while ffmpeg is still muxing.
                    jobProgress->report(
                        qBound(0, qRound(seconds / totalDurationSeconds * 100.0), 99), label);
                }
            }
        }
        const QByteArray err = process.readAllStandardError();
        if (!err.isEmpty()) {
            stderrTail += QString::fromLocal8Bit(err);
            if (stderrTail.size() > 8000) {
                stderrTail = stderrTail.right(8000);
            }
        }
    };

    while (process.state() != QProcess::NotRunning) {
        process.waitForReadyRead(100);
        pump();
        // AllEvents (not ExcludeUserInputEvents) so the overlay's Cancel button
        // receives clicks; the overlay swallows input to everything beneath it.
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        if (cancellationRequested()) {
            process.kill();
            process.waitForFinished(2000);
            endJob();
            return false;
        }
    }
    process.waitForFinished(200);
    pump();  // drain anything emitted between the last read and exit
    if (cancellationRequested()) {
        endJob();
        return false;
    }
    if (determinate) {
        jobProgress->report(100, label);
    }
    endJob();

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (error != nullptr) {
            const QString trimmed = stderrTail.trimmed();
            *error = trimmed.isEmpty()
                ? miacode::localizedText("media_tools.ffmpeg_exit_error").arg(process.exitCode())
                : miacode::LocalizedText(trimmed.right(2000));
        }
        return false;
    }
    return true;
}

bool probeMediaDurationSeconds(const QString& ffmpegPath, const QString& mediaPath, double* durationSeconds, miacode::LocalizedText* error)
{
    QStringList args;
    args << QStringLiteral("-hide_banner")
         << QStringLiteral("-i") << mediaPath
         << QStringLiteral("-f") << QStringLiteral("null")
         << QStringLiteral("-");

    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(ffmpegPath, args, QIODevice::ReadOnly);
    if (!process.waitForStarted(5000)) {
        if (error != nullptr) {
            *error = process.errorString();
        }
        return false;
    }
    if (!process.waitForFinished(30000)) {
        process.kill();
        process.waitForFinished(3000);
        if (error != nullptr) {
            *error = miacode::localizedText("media_tools.duration_probe_timeout");
        }
        return false;
    }

    const QString output = QString::fromLocal8Bit(process.readAll());
    static const QRegularExpression durationPattern(
        QStringLiteral(R"(Duration:\s*(\d+):(\d+):(\d+(?:\.\d+)?))")
    );
    const QRegularExpressionMatch match = durationPattern.match(output);
    if (!match.hasMatch()) {
        if (error != nullptr) {
            *error = miacode::localizedText("media_tools.duration_read_failed");
        }
        return false;
    }

    const double hours = match.captured(1).toDouble();
    const double minutes = match.captured(2).toDouble();
    const double seconds = match.captured(3).toDouble();
    const double totalSeconds = hours * 3600.0 + minutes * 60.0 + seconds;
    if (!(totalSeconds > 0.0)) {
        if (error != nullptr) {
            *error = miacode::localizedText("media_tools.duration_invalid");
        }
        return false;
    }
    if (durationSeconds != nullptr) {
        *durationSeconds = totalSeconds;
    }
    return true;
}

bool compressVideoUnder20Mb(
    const QString& ffmpegPath,
    const QString& videoPath,
    miacode::JobProgressService* jobProgress,
    miacode::LocalizedText* error,
    bool* cancelled = nullptr,
    bool* preservedCompressed = nullptr)
{
    if (preservedCompressed != nullptr) {
        *preservedCompressed = false;
    }
    const QFileInfo videoInfo(videoPath);
    const qint64 originalBytes = videoInfo.size();
    if (originalBytes > 0 && originalBytes < miacode::media::kPvCompressionHardLimitBytes) {
        if (error != nullptr) {
            *error = miacode::localizedText("media_tools.the_current_video_is_already");
        }
        return false;
    }
    const QString backupPath = videoInfo.dir().filePath(
        QStringLiteral("%1_bak.%2").arg(videoInfo.completeBaseName(), videoInfo.suffix())
    );
    const QString tempPath = videoInfo.dir().filePath(QStringLiteral(".miacode_video_compress_tmp.mp4"));
    QFile::remove(tempPath);
    if (!copyFileReplacing(videoPath, backupPath, error)) {
        return false;
    }

    double durationSeconds = 0.0;
    if (!probeMediaDurationSeconds(ffmpegPath, backupPath, &durationSeconds, error)) {
        return false;
    }

    QTemporaryDir passLogDirectory;
    if (!passLogDirectory.isValid()) {
        if (error != nullptr) {
            *error = miacode::localizedText("media_tools.pass_log_directory_failed");
        }
        return false;
    }

    miacode::media::PvCompressionPlan plan =
        miacode::media::makePvCompressionPlan(durationSeconds);
    qint64 compressedBytes = 0;
    bool encoded = false;
    for (int attempt = 0; attempt < 2; ++attempt) {
        QFile::remove(tempPath);
        const QString passLogPath = QDir(passLogDirectory.path()).filePath(
            QStringLiteral("x264-attempt-%1").arg(attempt));
        const QStringList firstPass = miacode::media::makePvCompressionPassArguments(
            backupPath, tempPath, passLogPath, plan, 1);
        const QStringList secondPass = miacode::media::makePvCompressionPassArguments(
            backupPath, tempPath, passLogPath, plan, 2);
        const bool firstPassOk = runFfmpegBlocking(
            ffmpegPath,
            firstPass,
            jobProgress,
            miacode::localizedText("media_tools.compressing_video"),
            miacode::localizedText("media_tools.compressing_video"),
            durationSeconds,
            error,
            cancelled);
        const bool secondPassOk = firstPassOk && runFfmpegBlocking(
            ffmpegPath,
            secondPass,
            jobProgress,
            miacode::localizedText("media_tools.compressing_video"),
            miacode::localizedText("media_tools.compressing_video"),
            durationSeconds,
            error,
            cancelled);
        if (!firstPassOk || !secondPassOk) {
            QFile::remove(tempPath);
            return false;
        }

        compressedBytes = QFileInfo(tempPath).size();
        if (miacode::media::isAcceptablePvCompressionOutput(originalBytes, compressedBytes)) {
            encoded = true;
            break;
        }
        if (attempt == 0 && compressedBytes >= miacode::media::kPvCompressionHardLimitBytes) {
            plan = miacode::media::adjustedPvCompressionPlan(plan, compressedBytes);
        } else {
            break;
        }
    }

    if (!encoded) {
        QFile::remove(tempPath);
        if (error != nullptr) {
            *error = compressedBytes >= miacode::media::kPvCompressionHardLimitBytes
                ? miacode::localizedText("media_tools.compressed_size_exceeded")
                : miacode::localizedText("media_tools.compressed_size_not_reduced");
        }
        return false;
    }
    if (replaceFileWithTemp(tempPath, videoPath, error)) {
        return true;
    }
    // Compression itself SUCCEEDED — only the in-place replace could not complete
    // (the original is still held open). Don't discard the finished clip: move it
    // beside the original under a clear, user-visible name so the work is kept and
    // the user can swap it in by hand after freeing the file. The original is left
    // untouched, and its bytes are also in the <base>_bak backup made up-front, so
    // nothing is lost. `error` currently holds replaceFileWithTemp's lock
    // diagnostic; prepend the "saved as …" note so both are shown.
    const QString preservedPath = videoInfo.dir().filePath(
        QStringLiteral("%1_compressed.%2").arg(videoInfo.completeBaseName(), videoInfo.suffix()));
    QFile::remove(preservedPath);
    if (QFile::rename(tempPath, preservedPath)) {
        if (preservedCompressed != nullptr) {
            *preservedCompressed = true;
        }
        if (error != nullptr) {
            const miacode::LocalizedText staged = *error;
            *error = miacode::localizedText("media_tools.compressed_but_replace_failed")
                         .arg(QDir::toNativeSeparators(preservedPath));
            if (!staged.isEmpty()) {
                *error = *error + QStringLiteral("\n\n") + staged;
            }
        }
    }
    // If even the rename-out failed, leave the temp in place (still recoverable)
    // and keep replaceFileWithTemp's error as-is.
    return false;
}

bool convertTrackTo44100Hz(
    const QString& ffmpegPath,
    const QString& trackPath,
    miacode::JobProgressService* jobProgress,
    miacode::LocalizedText* error,
    bool* cancelled = nullptr)
{
    const QFileInfo trackInfo(trackPath);
    const QString backupPath = trackInfo.dir().filePath(QStringLiteral("track_bak.mp3"));
    const QString tempPath = trackInfo.dir().filePath(QStringLiteral(".miacode_track_44100_tmp.mp3"));
    QFile::remove(tempPath);
    if (!copyFileReplacing(trackPath, backupPath, error)) {
        return false;
    }

    // Best-effort duration probe so the progress bar can read real percent;
    // a failed probe just degrades to the indeterminate busy bar.
    double trackDurationSeconds = 0.0;
    probeMediaDurationSeconds(ffmpegPath, backupPath, &trackDurationSeconds, nullptr);

    QStringList args;
    args << QStringLiteral("-hide_banner")
         << QStringLiteral("-y")
         << QStringLiteral("-i") << backupPath
         << QStringLiteral("-vn")
         << QStringLiteral("-ar") << QStringLiteral("44100")
         << QStringLiteral("-c:a") << QStringLiteral("libmp3lame")
         << QStringLiteral("-q:a") << QStringLiteral("2")
         << tempPath;
    if (!runFfmpegBlocking(
            ffmpegPath,
            args,
            jobProgress,
            miacode::localizedText("media_tools.processing_audio"),
            miacode::localizedText("media_tools.processing_audio"),
            trackDurationSeconds,
            error,
            cancelled)) {
        QFile::remove(tempPath);
        return false;
    }
    return replaceFileWithTemp(tempPath, trackPath, error);
}

bool prependTrackSilence(
    const QString& ffmpegPath,
    const QString& trackPath,
    double silenceSeconds,
    miacode::JobProgressService* jobProgress,
    miacode::LocalizedText* error,
    bool* cancelled = nullptr)
{
    const QFileInfo trackInfo(trackPath);
    const QString backupPath = trackInfo.dir().filePath(QStringLiteral("%1_bak.%2").arg(trackInfo.completeBaseName(), trackInfo.suffix()));
    const QString tempPath = trackInfo.dir().filePath(QStringLiteral(".miacode_track_prepend_tmp.%1").arg(trackInfo.suffix()));
    QFile::remove(tempPath);
    if (!copyFileReplacing(trackPath, backupPath, error)) {
        return false;
    }

    // Output runs for the original track plus the prepended silence; probe
    // the source so the progress bar can track real percent (best-effort).
    double inputDurationSeconds = 0.0;
    probeMediaDurationSeconds(ffmpegPath, backupPath, &inputDurationSeconds, nullptr);
    const double totalDurationSeconds =
        inputDurationSeconds > 0.0 ? inputDurationSeconds + silenceSeconds : 0.0;

    const QStringList args = miacode::media::makeAudioPrependArguments(
        backupPath, tempPath, trackInfo.suffix(), silenceSeconds);
    if (!runFfmpegBlocking(
            ffmpegPath,
            args,
            jobProgress,
            miacode::localizedText("media_tools.processing_audio"),
            miacode::localizedText("media_tools.processing_audio"),
            totalDurationSeconds,
            error,
            cancelled)) {
        QFile::remove(tempPath);
        return false;
    }
    return replaceFileWithTemp(tempPath, trackPath, error);
}

bool probeVideoDimensions(const QString& ffmpegPath, const QString& mediaPath, QSize* dimensions, miacode::LocalizedText* error)
{
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(
        ffmpegPath,
        {QStringLiteral("-hide_banner"), QStringLiteral("-i"), mediaPath},
        QIODevice::ReadOnly);
    if (!process.waitForStarted(5000)) {
        if (error != nullptr) {
            *error = process.errorString();
        }
        return false;
    }
    if (!process.waitForFinished(30000)) {
        process.kill();
        process.waitForFinished(3000);
        if (error != nullptr) {
            *error = QStringLiteral("Timed out while probing video dimensions.");
        }
        return false;
    }

    const QString output = QString::fromLocal8Bit(process.readAll());
    static const QRegularExpression dimensionsPattern(
        QStringLiteral(R"(Video:.*?\s(\d{2,})x(\d{2,})(?:[\s,]))"));
    const QRegularExpressionMatch match = dimensionsPattern.match(output);
    if (!match.hasMatch()) {
        if (error != nullptr) {
            *error = miacode::localizedText("media_tools.pv_geometry_probe_failed");
        }
        return false;
    }
    const QSize size(match.captured(1).toInt(), match.captured(2).toInt());
    if (!size.isValid()) {
        if (error != nullptr) {
            *error = miacode::localizedText("media_tools.pv_geometry_probe_failed");
        }
        return false;
    }
    if (dimensions != nullptr) {
        *dimensions = size;
    }
    return true;
}


bool prependPvBlack(
    const QString& ffmpegPath,
    const QString& pvPath,
    double silenceSeconds,
    miacode::JobProgressService* jobProgress,
    miacode::LocalizedText* error,
    bool* cancelled = nullptr)
{
    const QFileInfo pvInfo(pvPath);
    const QString backupPath = pvInfo.dir().filePath(
        QStringLiteral("%1_bak.%2").arg(pvInfo.completeBaseName(), pvInfo.suffix())
    );
    const QString tempPath = pvInfo.dir().filePath(QStringLiteral(".miacode_pv_prepend_tmp.mp4"));
    QFile::remove(tempPath);
    if (!copyFileReplacing(pvPath, backupPath, error)) {
        return false;
    }

    // Keep the source geometry/timing and size the encode against the original
    // average bitrate instead of upscaling every PV to a fixed 1080p CRF encode.
    double inputDurationSeconds = 0.0;
    if (!probeMediaDurationSeconds(ffmpegPath, backupPath, &inputDurationSeconds, error)) {
        return false;
    }
    QSize inputDimensions;
    if (!probeVideoDimensions(ffmpegPath, backupPath, &inputDimensions, error)) {
        return false;
    }
    const double totalDurationSeconds =
        inputDurationSeconds + silenceSeconds;

    QTemporaryDir passLogDirectory;
    if (!passLogDirectory.isValid()) {
        if (error != nullptr) {
            *error = QStringLiteral("Could not create the two-pass log directory.");
        }
        return false;
    }

    miacode::media::PvCompressionPlan plan = miacode::media::makePvPrependCompressionPlan(
        pvInfo.size(),
        inputDurationSeconds,
        silenceSeconds);
    qint64 outputBytes = 0;
    bool encoded = false;
    for (int attempt = 0; attempt < 2; ++attempt) {
        QFile::remove(tempPath);
        const QString passLogPath = QDir(passLogDirectory.path()).filePath(
            QStringLiteral("x264-prepend-attempt-%1").arg(attempt));
        const QStringList firstPass = miacode::media::makePvPrependPassArguments(
            backupPath, tempPath, passLogPath, plan, silenceSeconds, 1);
        const QStringList secondPass = miacode::media::makePvPrependPassArguments(
            backupPath, tempPath, passLogPath, plan, silenceSeconds, 2);
        if (!runFfmpegBlocking(
                ffmpegPath,
                firstPass,
                jobProgress,
                miacode::localizedText("media_tools.processing_pv_mp4"),
                miacode::localizedText("media_tools.processing_pv_mp4"),
                totalDurationSeconds,
                error,
                cancelled)
            || !runFfmpegBlocking(
                ffmpegPath,
                secondPass,
                jobProgress,
                miacode::localizedText("media_tools.processing_pv_mp4"),
                miacode::localizedText("media_tools.processing_pv_mp4"),
                totalDurationSeconds,
                error,
                cancelled)) {
            QFile::remove(tempPath);
            return false;
        }

        double outputDurationSeconds = 0.0;
        QSize outputDimensions;
        if (!probeMediaDurationSeconds(ffmpegPath, tempPath, &outputDurationSeconds, error)
            || !probeVideoDimensions(ffmpegPath, tempPath, &outputDimensions, error)) {
            QFile::remove(tempPath);
            return false;
        }
        if (outputDimensions != inputDimensions
            || std::abs(outputDurationSeconds - totalDurationSeconds) > 0.25) {
            QFile::remove(tempPath);
            if (error != nullptr) {
                *error = miacode::localizedText("media_tools.prepend_pv_geometry_mismatch");
            }
            return false;
        }

        outputBytes = QFileInfo(tempPath).size();
        if (miacode::media::isAcceptablePvPrependOutput(plan, outputBytes)) {
            encoded = true;
            break;
        }
        if (attempt == 0) {
            plan = miacode::media::adjustedPvCompressionPlan(plan, outputBytes);
        }
    }

    if (!encoded) {
        QFile::remove(tempPath);
        if (error != nullptr) {
            *error = miacode::localizedText("media_tools.prepend_pv_size_limit_failed");
        }
        return false;
    }
    return replaceFileWithTemp(tempPath, pvPath, error);
}

constexpr int kAlignmentAudioSampleRate = 12000;
constexpr int kAlignmentEnvelopeRate = 50;

bool alignmentCancellationRequested(
    miacode::JobProgressService* jobProgress, quint64 jobToken, bool* cancelled)
{
    if (jobProgress->active() && jobProgress->token() == jobToken
        && !jobProgress->cancelRequested()) {
        return false;
    }
    if (cancelled != nullptr) {
        *cancelled = true;
    }
    return true;
}

struct VideoAlignmentMediaInfo
{
    double durationSeconds = 0.0;
    qint64 videoBitrate = 0;
    qint64 audioBitrate = 0;
    bool hasAudio = false;
};

bool probeVideoAlignmentMediaInfo(
    const QString& ffmpegPath,
    const QString& mediaPath,
    VideoAlignmentMediaInfo* mediaInfo,
    miacode::JobProgressService* jobProgress,
    quint64 jobToken,
    miacode::LocalizedText* error,
    bool* cancelled)
{
    if (alignmentCancellationRequested(jobProgress, jobToken, cancelled)) {
        return false;
    }
    jobProgress->reportIndeterminate(
        miacode::localizedText("media_tools.analyzing_audio_alignment"));
    QElapsedTimer timer;
    timer.start();

    // Reading the input header avoids decoding the entire video just to obtain its bitrate.
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(ffmpegPath, {
        QStringLiteral("-hide_banner"), QStringLiteral("-nostdin"),
        QStringLiteral("-i"), mediaPath
    }, QIODevice::ReadOnly);
    if (!process.waitForStarted(5000)) {
        if (error != nullptr) {
            *error = process.errorString();
        }
        return false;
    }
    QString output;
    while (process.state() != QProcess::NotRunning) {
        process.waitForReadyRead(100);
        output += QString::fromLocal8Bit(process.readAll());
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        if (alignmentCancellationRequested(jobProgress, jobToken, cancelled)) {
            process.kill();
            process.waitForFinished(2000);
            return false;
        }
        if (timer.hasExpired(30000)) {
            process.kill();
            process.waitForFinished(2000);
            if (error != nullptr) {
                *error = miacode::localizedText("media_tools.duration_probe_timeout");
            }
            return false;
        }
    }
    output += QString::fromLocal8Bit(process.readAll());
    if (alignmentCancellationRequested(jobProgress, jobToken, cancelled)) {
        return false;
    }

    static const QRegularExpression durationPattern(
        QStringLiteral(R"(Duration:\s*(\d+):(\d+):(\d+(?:\.\d+)?))"));
    static const QRegularExpression videoPattern(
        QStringLiteral(R"(^\s*Stream #0:\d+[^\r\n]*?: Video:([^\r\n]*))"),
        QRegularExpression::MultilineOption);
    static const QRegularExpression audioPattern(
        QStringLiteral(R"(^\s*Stream #0:\d+[^\r\n]*?: Audio:([^\r\n]*))"),
        QRegularExpression::MultilineOption);
    static const QRegularExpression dimensionsPattern(
        QStringLiteral(R"(\s(\d+)x(\d+)(?:[\s,]))"));
    static const QRegularExpression bitratePattern(
        QStringLiteral(R"((\d+(?:\.\d+)?)\s+kb/s)"));
    const QRegularExpressionMatch durationMatch = durationPattern.match(output);
    const QRegularExpressionMatch videoMatch = videoPattern.match(output);
    const QRegularExpressionMatch dimensionsMatch = dimensionsPattern.match(videoMatch.captured(1));
    // ffmpeg exits with code 1 when no output is requested; validate the header instead.
    if (process.exitStatus() != QProcess::NormalExit
        || !durationMatch.hasMatch() || !videoMatch.hasMatch() || !dimensionsMatch.hasMatch()) {
        if (error != nullptr) {
            *error = output.trimmed().isEmpty()
                ? miacode::localizedText("media_tools.duration_read_failed")
                : miacode::LocalizedText(output.trimmed().right(2000));
        }
        return false;
    }

    VideoAlignmentMediaInfo info;
    info.durationSeconds = durationMatch.captured(1).toDouble() * 3600.0
        + durationMatch.captured(2).toDouble() * 60.0
        + durationMatch.captured(3).toDouble();
    const QSize dimensions(dimensionsMatch.captured(1).toInt(), dimensionsMatch.captured(2).toInt());
    if (!(info.durationSeconds > 0.0) || !dimensions.isValid()) {
        if (error != nullptr) {
            *error = miacode::localizedText("media_tools.duration_read_failed");
        }
        return false;
    }
    const QRegularExpressionMatch videoBitrateMatch = bitratePattern.match(videoMatch.captured(1));
    if (videoBitrateMatch.hasMatch()) {
        info.videoBitrate = qRound64(videoBitrateMatch.captured(1).toDouble() * 1000.0);
    }
    const QRegularExpressionMatch audioMatch = audioPattern.match(output);
    info.hasAudio = audioMatch.hasMatch();
    const QRegularExpressionMatch audioBitrateMatch = bitratePattern.match(audioMatch.captured(1));
    if (audioBitrateMatch.hasMatch()) {
        info.audioBitrate = qRound64(audioBitrateMatch.captured(1).toDouble() * 1000.0);
    }
    *mediaInfo = info;
    miacode::debug_log::appendLine(
        miacode::debug_log::Channel::Runtime, QStringLiteral("media/audio_alignment"),
        QStringLiteral("probed file=%1 seconds=%2 video_bps=%3 audio_bps=%4 has_audio=%5 elapsed_ms=%6")
            .arg(QFileInfo(mediaPath).fileName()).arg(info.durationSeconds, 0, 'f', 3)
            .arg(info.videoBitrate).arg(info.audioBitrate).arg(info.hasAudio).arg(timer.elapsed()));
    return true;
}

bool decodeAudioStreamWithFfmpeg(
    const QString& ffmpegPath,
    const QString& mediaPath,
    miacode::latency_analysis::DecodedAudio* decoded,
    miacode::JobProgressService* jobProgress,
    quint64 jobToken,
    miacode::LocalizedText* error,
    bool* cancelled)
{
    if (alignmentCancellationRequested(jobProgress, jobToken, cancelled)) {
        return false;
    }
    jobProgress->reportIndeterminate(
        miacode::localizedText("media_tools.analyzing_audio_alignment"));
    QElapsedTimer timer;
    timer.start();

    QProcess process;
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start(ffmpegPath, {
        QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-i"), mediaPath,
        QStringLiteral("-map"), QStringLiteral("0:a:0"),
        QStringLiteral("-vn"), QStringLiteral("-sn"), QStringLiteral("-dn"),
        QStringLiteral("-ac"), QStringLiteral("1"),
        QStringLiteral("-ar"), QString::number(kAlignmentAudioSampleRate),
        QStringLiteral("-f"), QStringLiteral("f32le"), QStringLiteral("pipe:1")
    }, QIODevice::ReadOnly);
    if (!process.waitForStarted(5000)) {
        if (error != nullptr) {
            *error = process.errorString();
        }
        miacode::debug_log::appendLine(
            miacode::debug_log::Channel::Runtime, QStringLiteral("media/audio_alignment"),
            QStringLiteral("decode_start_failed file=%1 error=%2")
                .arg(QFileInfo(mediaPath).fileName(), process.errorString()));
        return false;
    }

    QByteArray pcmBytes;
    QString stderrTail;
    const auto pump = [&]() {
        pcmBytes.append(process.readAllStandardOutput());
        stderrTail += QString::fromLocal8Bit(process.readAllStandardError());
        if (stderrTail.size() > 8000) {
            stderrTail = stderrTail.right(8000);
        }
    };
    while (process.state() != QProcess::NotRunning) {
        process.waitForReadyRead(100);
        pump();
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        if (alignmentCancellationRequested(jobProgress, jobToken, cancelled)) {
            process.kill();
            process.waitForFinished(2000);
            return false;
        }
    }
    pump();
    if (alignmentCancellationRequested(jobProgress, jobToken, cancelled)) {
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (error != nullptr && !stderrTail.trimmed().isEmpty()) {
            *error = miacode::LocalizedText(stderrTail.trimmed().right(2000));
        }
        miacode::debug_log::appendLine(
            miacode::debug_log::Channel::Runtime, QStringLiteral("media/audio_alignment"),
            QStringLiteral("decode_failed file=%1 exit=%2 error=%3")
                .arg(QFileInfo(mediaPath).fileName()).arg(process.exitCode()).arg(stderrTail.trimmed()));
        return false;
    }

    const qsizetype sampleCount = pcmBytes.size() / static_cast<qsizetype>(sizeof(float));
    if (sampleCount < kAlignmentAudioSampleRate) {
        if (error != nullptr) {
            *error = miacode::localizedText("media_tools.video_alignment_offset_failed");
        }
        return false;
    }
    decoded->samples.resize(sampleCount);
    std::memcpy(
        decoded->samples.data(), pcmBytes.constData(),
        static_cast<size_t>(sampleCount) * sizeof(float));
    decoded->sampleRate = kAlignmentAudioSampleRate;
    decoded->durationSeconds = static_cast<double>(sampleCount) / kAlignmentAudioSampleRate;
    miacode::debug_log::appendLine(
        miacode::debug_log::Channel::Runtime, QStringLiteral("media/audio_alignment"),
        QStringLiteral("decoded file=%1 seconds=%2 elapsed_ms=%3")
            .arg(QFileInfo(mediaPath).fileName()).arg(decoded->durationSeconds, 0, 'f', 3)
            .arg(timer.elapsed()));
    return true;
}

QVector<double> buildAlignmentEnvelope(
    const miacode::latency_analysis::DecodedAudio& decoded,
    miacode::JobProgressService* jobProgress,
    quint64 jobToken,
    bool* cancelled)
{
    QVector<double> envelope;
    if (decoded.samples.isEmpty() || decoded.sampleRate <= 0
        || alignmentCancellationRequested(jobProgress, jobToken, cancelled)) {
        return envelope;
    }

    const int windowSamples = qMax(1, decoded.sampleRate / kAlignmentEnvelopeRate);
    envelope.reserve((decoded.samples.size() + windowSamples - 1) / windowSamples);
    QElapsedTimer eventTimer;
    eventTimer.start();
    double previousRms = 0.0;
    for (qsizetype start = 0; start < decoded.samples.size(); start += windowSamples) {
        const qsizetype end = qMin(start + windowSamples, decoded.samples.size());
        double squareSum = 0.0;
        for (qsizetype sampleIndex = start; sampleIndex < end; ++sampleIndex) {
            const double sample = decoded.samples.at(sampleIndex);
            squareSum += sample * sample;
        }
        const double rms = std::sqrt(squareSum / static_cast<double>(end - start));
        envelope.append(qMax(0.0, rms - previousRms * 0.85));
        previousRms = rms;
        if (envelope.size() % 128 == 0 && eventTimer.hasExpired(40)) {
            QCoreApplication::processEvents(QEventLoop::AllEvents);
            eventTimer.restart();
            if (alignmentCancellationRequested(jobProgress, jobToken, cancelled)) {
                return {};
            }
        }
    }
    return envelope;
}

bool estimateVideoAudioOffset(
    const miacode::latency_analysis::DecodedAudio& trackAudio,
    const miacode::latency_analysis::DecodedAudio& videoAudio,
    double* offsetSeconds,
    miacode::JobProgressService* jobProgress,
    quint64 jobToken,
    bool* cancelled)
{
    if (alignmentCancellationRequested(jobProgress, jobToken, cancelled)) {
        return false;
    }
    QElapsedTimer timer;
    timer.start();
    const miacode::LocalizedText label = miacode::localizedText("media_tools.analyzing_audio_alignment");
    jobProgress->reportIndeterminate(label);
    const QVector<double> trackEnvelope = buildAlignmentEnvelope(
        trackAudio, jobProgress, jobToken, cancelled);
    const QVector<double> videoEnvelope = buildAlignmentEnvelope(
        videoAudio, jobProgress, jobToken, cancelled);
    if (alignmentCancellationRequested(jobProgress, jobToken, cancelled)) {
        return false;
    }
    if (trackEnvelope.size() < kAlignmentEnvelopeRate
        || videoEnvelope.size() < kAlignmentEnvelopeRate) {
        return false;
    }

    const int trackSize = static_cast<int>(trackEnvelope.size());
    const int videoSize = static_cast<int>(videoEnvelope.size());
    // Requiring the entire shorter clip would leave equal short clips at lag zero.
    const int minimumOverlap = qMin(
        3 * kAlignmentEnvelopeRate,
        qMax(kAlignmentEnvelopeRate, qMin(trackSize, videoSize) / 2));
    const int maxLag = qMin(
        15 * kAlignmentEnvelopeRate, qMax(trackSize, videoSize) - minimumOverlap);

    // Window sums and variances are constant-time; only the dot product varies by lag.
    QVector<double> trackSums(trackSize + 1, 0.0);
    QVector<double> trackSquareSums(trackSize + 1, 0.0);
    QVector<double> videoSums(videoSize + 1, 0.0);
    QVector<double> videoSquareSums(videoSize + 1, 0.0);
    for (int index = 0; index < trackSize; ++index) {
        const double value = trackEnvelope.at(index);
        trackSums[index + 1] = trackSums.at(index) + value;
        trackSquareSums[index + 1] = trackSquareSums.at(index) + value * value;
    }
    for (int index = 0; index < videoSize; ++index) {
        const double value = videoEnvelope.at(index);
        videoSums[index + 1] = videoSums.at(index) + value;
        videoSquareSums[index + 1] = videoSquareSums.at(index) + value * value;
    }

    QVector<double> scores(maxLag * 2 + 1, -1.0);
    const double* const trackValues = trackEnvelope.constData();
    const double* const videoValues = videoEnvelope.constData();
    QElapsedTimer eventTimer;
    eventTimer.start();
    const auto keepAnalyzing = [&]() {
        if (eventTimer.hasExpired(40)) {
            QCoreApplication::processEvents(QEventLoop::AllEvents);
            eventTimer.restart();
        }
        return !alignmentCancellationRequested(jobProgress, jobToken, cancelled);
    };
    double bestScore = -1.0;
    int bestLag = 0;
    int bestOverlap = 0;
    for (int lag = -maxLag; lag <= maxLag; ++lag) {
        if (!keepAnalyzing()) {
            return false;
        }
        jobProgress->report((lag + maxLag) * 100 / (maxLag * 2 + 1), label);
        const int trackStart = qMax(0, -lag);
        const int trackEnd = qMin(trackSize, videoSize - lag);
        const int count = trackEnd - trackStart;
        if (count < minimumOverlap) {
            continue;
        }

        const int videoStart = trackStart + lag;
        const int videoEnd = trackEnd + lag;
        const double trackSum = trackSums.at(trackEnd) - trackSums.at(trackStart);
        const double videoSum = videoSums.at(videoEnd) - videoSums.at(videoStart);
        const double trackSquareSum = trackSquareSums.at(trackEnd) - trackSquareSums.at(trackStart);
        const double videoSquareSum = videoSquareSums.at(videoEnd) - videoSquareSums.at(videoStart);
        const double trackVariance = trackSquareSum - trackSum * trackSum / count;
        const double videoVariance = videoSquareSum - videoSum * videoSum / count;
        if (trackVariance <= 1e-12 || videoVariance <= 1e-12) {
            continue;
        }

        double productSum = 0.0;
        for (int blockStart = trackStart; blockStart < trackEnd; blockStart += 4096) {
            const int blockEnd = qMin(blockStart + 4096, trackEnd);
            for (int index = blockStart; index < blockEnd; ++index) {
                productSum += trackValues[index] * videoValues[index + lag];
            }
            if (!keepAnalyzing()) {
                return false;
            }
        }
        const double covariance = productSum - trackSum * videoSum / count;
        const double score = covariance / std::sqrt(trackVariance * videoVariance);
        scores[lag + maxLag] = score;
        if (score > bestScore) {
            bestScore = score;
            bestLag = lag;
            bestOverlap = count;
        }
    }
    if (!keepAnalyzing()) {
        return false;
    }
    jobProgress->report(100, label);
    if (bestScore < 0.35) {
        miacode::debug_log::appendLine(
            miacode::debug_log::Channel::Runtime, QStringLiteral("media/audio_alignment"),
            QStringLiteral("match_rejected score=%1 overlap_seconds=%2 elapsed_ms=%3")
                .arg(bestScore, 0, 'f', 4).arg(static_cast<double>(bestOverlap) / kAlignmentEnvelopeRate)
                .arg(timer.elapsed()));
        return false;
    }

    double refinedLag = bestLag;
    const int scoreIndex = bestLag + maxLag;
    if (scoreIndex > 0 && scoreIndex + 1 < scores.size()) {
        const double left = scores.at(scoreIndex - 1);
        const double center = scores.at(scoreIndex);
        const double right = scores.at(scoreIndex + 1);
        const double curvature = left - 2.0 * center + right;
        if (left >= 0.0 && right >= 0.0 && curvature < -1e-9) {
            refinedLag += qBound(-0.5, 0.5 * (left - right) / curvature, 0.5);
        }
    }
    *offsetSeconds = refinedLag / kAlignmentEnvelopeRate;
    miacode::debug_log::appendLine(
        miacode::debug_log::Channel::Runtime, QStringLiteral("media/audio_alignment"),
        QStringLiteral("matched offset_seconds=%1 score=%2 overlap_seconds=%3 max_lag_seconds=%4 elapsed_ms=%5")
            .arg(*offsetSeconds, 0, 'f', 6).arg(bestScore, 0, 'f', 4)
            .arg(static_cast<double>(bestOverlap) / kAlignmentEnvelopeRate)
            .arg(static_cast<double>(maxLag) / kAlignmentEnvelopeRate).arg(timer.elapsed()));
    return true;
}

QString resolveChartMp4Path(const QString& chartPath, const QString& videoFieldValue)
{
    if (chartPath.isEmpty()) {
        return QString();
    }
    const QString chartDirectory = QFileInfo(chartPath).absolutePath();
    const QString trimmedField = videoFieldValue.trimmed();
    if (!trimmedField.isEmpty()) {
        const QFileInfo fieldInfo(trimmedField);
        const QString candidate = fieldInfo.isAbsolute()
            ? QDir::cleanPath(trimmedField)
            : QDir::cleanPath(QDir(chartDirectory).filePath(trimmedField));
        if (QFileInfo(candidate).isFile() && miacode::chart_assets::isVideoBackgroundPath(candidate)) {
            return candidate;
        }
    }

    const QDir directory(chartDirectory);
    for (const QString& name : {QStringLiteral("bg.mp4"), QStringLiteral("pv.mp4")}) {
        const QString candidate = directory.filePath(name);
        if (QFileInfo(candidate).isFile()) {
            return QDir::cleanPath(candidate);
        }
    }
    return QString();
}

bool alignVideoStreams(
    const QString& ffmpegPath,
    const QString& videoPath,
    double videoAudioOffsetSeconds,
    const VideoAlignmentMediaInfo& mediaInfo,
    miacode::JobProgressService* jobProgress,
    quint64 jobToken,
    miacode::LocalizedText* error,
    bool* cancelled)
{
    if (alignmentCancellationRequested(jobProgress, jobToken, cancelled)) {
        return false;
    }
    const QFileInfo videoInfo(videoPath);
    const qint64 inputBytes = videoInfo.size();
    // Reserve muxing overhead only when estimating missing stream bitrates.
    const qint64 payloadBitrate = static_cast<qint64>(
        static_cast<double>(inputBytes) * 8.0 / mediaInfo.durationSeconds * 0.98);
    qint64 audioBitrate = mediaInfo.audioBitrate;
    if (audioBitrate <= 0) {
        const qint64 availableAudioBitrate = mediaInfo.videoBitrate > 0
            ? payloadBitrate - mediaInfo.videoBitrate
            : payloadBitrate / 10;
        audioBitrate = qBound(qint64{16000}, availableAudioBitrate, qint64{128000});
    }
    const qint64 videoBitrate = mediaInfo.videoBitrate > 0
        ? mediaInfo.videoBitrate
        : qMax<qint64>(32000, payloadBitrate - audioBitrate);
    miacode::debug_log::appendLine(
        miacode::debug_log::Channel::Runtime, QStringLiteral("media/audio_alignment"),
        QStringLiteral("encoding file=%1 offset_seconds=%2 source_video_bps=%3 source_audio_bps=%4 target_video_bps=%5 target_audio_bps=%6 input_bytes=%7")
            .arg(videoInfo.fileName()).arg(videoAudioOffsetSeconds, 0, 'f', 3)
            .arg(mediaInfo.videoBitrate).arg(mediaInfo.audioBitrate)
            .arg(videoBitrate).arg(audioBitrate).arg(inputBytes));
    const QString backupPath = videoInfo.dir().filePath(
        QStringLiteral("%1_bak.%2").arg(videoInfo.completeBaseName(), videoInfo.suffix()));
    QTemporaryDir tempDirectory(videoInfo.dir().filePath(QStringLiteral(".miacode_pv_align_XXXXXX")));
    if (!tempDirectory.isValid()) {
        if (error != nullptr) {
            *error = miacode::localizedText("media_tools.failed_to_write_file_1")
                .arg(videoInfo.absolutePath());
        }
        return false;
    }
    const QString tempPath = tempDirectory.filePath(QStringLiteral("aligned.mp4"));
    const double adjustmentSeconds = qAbs(videoAudioOffsetSeconds);
    const QString adjustment = QString::number(adjustmentSeconds, 'f', 6);
    QString videoFilter;
    QString audioFilter;
    if (videoAudioOffsetSeconds > 0.0) {
        videoFilter = QStringLiteral("trim=start=%1,setpts=PTS-%1/TB,scale=trunc(iw/2)*2:trunc(ih/2)*2,format=yuv420p")
                          .arg(adjustment);
        audioFilter = QStringLiteral("atrim=start=%1,asetpts=PTS-%1/TB").arg(adjustment);
    } else {
        const QString delayMilliseconds = QString::number(qRound64(adjustmentSeconds * 1000.0));
        videoFilter = QStringLiteral("tpad=start_mode=add:start_duration=%1:color=black,scale=trunc(iw/2)*2:trunc(ih/2)*2,format=yuv420p")
                          .arg(adjustment);
        audioFilter = QStringLiteral("adelay=%1:all=1").arg(delayMilliseconds);
    }

    QStringList args;
    args << QStringLiteral("-hide_banner")
         << QStringLiteral("-y")
         << QStringLiteral("-i") << videoPath
         << QStringLiteral("-filter_complex")
         << QStringLiteral("[0:v:0]%1[v];[0:a:0]%2[a]").arg(videoFilter, audioFilter)
         << QStringLiteral("-map") << QStringLiteral("[v]")
         << QStringLiteral("-map") << QStringLiteral("[a]")
         << QStringLiteral("-c:v") << QStringLiteral("libx264")
         << QStringLiteral("-preset") << QStringLiteral("veryfast")
         << QStringLiteral("-b:v") << QString::number(videoBitrate)
         << QStringLiteral("-pix_fmt") << QStringLiteral("yuv420p")
         << QStringLiteral("-c:a") << QStringLiteral("aac")
         << QStringLiteral("-b:a") << QString::number(audioBitrate)
         << QStringLiteral("-movflags") << QStringLiteral("+faststart")
         << tempPath;
    if (!runFfmpegBlocking(
            ffmpegPath,
            args,
            jobProgress,
            miacode::localizedText("media_tools.align_pv_to_audio"),
            miacode::localizedText("media_tools.processing_pv_mp4"),
            qMax(0.0, mediaInfo.durationSeconds - videoAudioOffsetSeconds),
            error,
            cancelled,
            jobToken)) {
        return false;
    }
    if (alignmentCancellationRequested(jobProgress, jobToken, cancelled)) {
        return false;
    }
    miacode::debug_log::appendLine(
        miacode::debug_log::Channel::Runtime, QStringLiteral("media/audio_alignment"),
        QStringLiteral("encoded file=%1 input_bytes=%2 output_bytes=%3")
            .arg(videoInfo.fileName()).arg(inputBytes).arg(QFileInfo(tempPath).size()));
    // Leave the original and any previous backup untouched until encoding succeeds.
    if (!copyFileReplacing(videoPath, backupPath, error)
        || alignmentCancellationRequested(jobProgress, jobToken, cancelled)) {
        return false;
    }
    return replaceFileWithTemp(tempPath, videoPath, error);
}

} // namespace

void miacode::runtime::MediaJobsHost::onCompressBackgroundVideo()
{
    MC_OP("miacode::runtime::MediaJobsHost::onCompressBackgroundVideo");
    miacode::UiRequestService* const requests = session_.uiRequestService();
    if (requests == nullptr) {
        return;
    }
    const miacode::LocalizedText title = miacode::localizedText("media_tools.compress_video");
    const QString chartDirPath = resolveCurrentChartDirectory();
    if (chartDirPath.isEmpty()) {
        requests->postNotice(
            miacode::NoticeSeverity::Warning, title,
            miacode::localizedText("media_tools.open_or_save_a_chart"));
        return;
    }

    const QString videoPath = miacode::chart_assets::resolveChartVideoPath(
        session_.currentFilePath_, session_.applicationServices_.workspace().document().videoPath);
    if (!QFileInfo::exists(videoPath)) {
        requests->postNotice(
            miacode::NoticeSeverity::Warning, title,
            miacode::localizedText("media_tools.no_background_mp4_video_was"));
        return;
    }

    const QFileInfo videoInfo(videoPath);
    // Size gate up-front: if the video is already under 20 MB there is nothing
    // to compress, so say so immediately instead of making the user confirm
    // first and only then discovering there's no work to do.
    const qint64 videoSizeBytes = videoInfo.size();
    if (videoSizeBytes > 0
        && videoSizeBytes < miacode::media::kPvCompressionHardLimitBytes) {
        requests->postNotice(
            miacode::NoticeSeverity::Information, title,
            miacode::localizedText("media_tools.the_current_video_is_already_2")
                .arg(QLocale().formattedDataSize(videoSizeBytes)));
        return;
    }
    const QString backupName =
        QStringLiteral("%1_bak.%2").arg(videoInfo.completeBaseName(), videoInfo.suffix());
    requests->requestConfirmation(
        title,
        miacode::localizedText("media_tools.compress_1_under_20_mib")
            .arg(videoInfo.fileName(), backupName),
        miacode::localizedText("media_tools.compress_video"),
        [this, title, videoPath, backupName](bool accepted) {
            if (accepted) {
                runCompressBackgroundVideo(title, videoPath, backupName);
            }
        });
}

void miacode::runtime::MediaJobsHost::onAlignBackgroundVideoToTrack()
{
    MC_OP("miacode::runtime::MediaJobsHost::onAlignBackgroundVideoToTrack");
    miacode::UiRequestService* const requests = session_.uiRequestService();
    miacode::JobProgressService* const jobProgress = session_.jobProgressService();
    if (requests == nullptr || jobProgress == nullptr || jobProgress->active()) {
        return;
    }
    const miacode::LocalizedText title = miacode::localizedText("media_tools.align_pv_to_audio");
    const QString chartDirPath = resolveCurrentChartDirectory();
    if (chartDirPath.isEmpty()) {
        requests->postNotice(
            miacode::NoticeSeverity::Warning, title,
            miacode::localizedText("media_tools.open_or_save_a_chart"));
        return;
    }

    const QString trackPath = resolveLatencyDetectorTrackPath();
    if (trackPath.isEmpty() || !QFileInfo::exists(trackPath)) {
        requests->postNotice(
            miacode::NoticeSeverity::Warning, title,
            miacode::localizedText("media_tools.track_mp3_was_not_found"));
        return;
    }
    const QString videoPath = resolveChartMp4Path(
        session_.currentFilePath_, session_.applicationServices_.workspace().document().videoPath);
    if (videoPath.isEmpty()) {
        requests->postNotice(
            miacode::NoticeSeverity::Warning, title,
            miacode::localizedText("media_tools.no_background_mp4_video_was"));
        return;
    }

    const QString ffmpegPath = resolveMediaToolFfmpegExecutable();
    if (ffmpegPath.isEmpty()) {
        requests->postNotice(
            miacode::NoticeSeverity::Error, title,
            miacode::localizedText("media_tools.ffmpeg_was_not_found_place"));
        return;
    }

    auto& workspace = session_.applicationServices_.workspace();
    const miacode::ChartWorkspaceSnapshot snapshot = workspace.snapshot();
    const auto workspaceStillCurrent = [&workspace, snapshot]() {
        const miacode::ChartWorkspaceSnapshot current = workspace.snapshot();
        return current.revision == snapshot.revision
            && current.documentOpenGeneration == snapshot.documentOpenGeneration
            && current.filePath == snapshot.filePath;
    };
    // One token covers probing, decode, correlation and encoding; cancellation survives stage changes.
    const quint64 jobToken = jobProgress->begin(
        title, miacode::localizedText("media_tools.analyzing_audio_alignment"), /*cancellable=*/true);
    const QMetaObject::Connection workspaceConnection = QObject::connect(
        &workspace, &miacode::ChartWorkspace::changed, jobProgress,
        [jobProgress, jobToken, workspaceStillCurrent](quint64) {
            if (jobProgress->token() == jobToken && !workspaceStillCurrent()) {
                jobProgress->requestCancel();
            }
        });
    const auto finishJob = qScopeGuard([jobProgress, jobToken, workspaceConnection]() {
        QObject::disconnect(workspaceConnection);
        if (jobProgress->token() == jobToken) {
            jobProgress->end();
        }
    });
    if (!workspaceStillCurrent()) {
        jobProgress->requestCancel();
    }

    VideoAlignmentMediaInfo videoMediaInfo;
    miacode::LocalizedText error;
    bool cancelled = false;
    if (!probeVideoAlignmentMediaInfo(
            ffmpegPath, videoPath, &videoMediaInfo, jobProgress, jobToken, &error, &cancelled)) {
        requests->postNotice(
            cancelled ? miacode::NoticeSeverity::Information : miacode::NoticeSeverity::Warning,
            title, cancelled ? miacode::localizedText("media_tools.video_alignment_cancelled") : error);
        return;
    }
    if (!videoMediaInfo.hasAudio) {
        requests->postNotice(
            miacode::NoticeSeverity::Warning, title,
            miacode::localizedText("media_tools.video_audio_missing"));
        return;
    }

    miacode::latency_analysis::DecodedAudio trackAudio;
    miacode::latency_analysis::DecodedAudio videoAudio;
    error = miacode::localizedText("media_tools.video_alignment_offset_failed");
    if (!decodeAudioStreamWithFfmpeg(
            ffmpegPath, videoPath, &videoAudio, jobProgress, jobToken, &error, &cancelled)) {
        requests->postNotice(
            cancelled ? miacode::NoticeSeverity::Information : miacode::NoticeSeverity::Warning,
            title, cancelled ? miacode::localizedText("media_tools.video_alignment_cancelled") : error);
        return;
    }
    error = miacode::localizedText("media_tools.video_alignment_offset_failed");
    if (!decodeAudioStreamWithFfmpeg(
            ffmpegPath, trackPath, &trackAudio, jobProgress, jobToken, &error, &cancelled)) {
        requests->postNotice(
            cancelled ? miacode::NoticeSeverity::Information : miacode::NoticeSeverity::Warning,
            title, cancelled ? miacode::localizedText("media_tools.video_alignment_cancelled") : error);
        return;
    }

    double videoAudioOffsetSeconds = 0.0;
    if (!estimateVideoAudioOffset(
            trackAudio, videoAudio, &videoAudioOffsetSeconds, jobProgress, jobToken, &cancelled)) {
        requests->postNotice(
            cancelled ? miacode::NoticeSeverity::Information : miacode::NoticeSeverity::Warning,
            title, cancelled ? miacode::localizedText("media_tools.video_alignment_cancelled")
                             : miacode::localizedText("media_tools.video_alignment_offset_failed"));
        return;
    }
    if (alignmentCancellationRequested(jobProgress, jobToken, &cancelled)) {
        return;
    }
    if (qAbs(videoAudioOffsetSeconds) < 1.0 / kAlignmentEnvelopeRate) {
        requests->postNotice(
            miacode::NoticeSeverity::Information, title,
            miacode::localizedText("media_tools.video_alignment_already_aligned")
                .arg(videoAudioOffsetSeconds, 0, 'f', 3));
        return;
    }

    releasePreviewMediaForFileOperation();
    const bool aligned = alignVideoStreams(
        ffmpegPath, videoPath, videoAudioOffsetSeconds, videoMediaInfo,
        jobProgress, jobToken, &error, &cancelled);
    const miacode::ChartWorkspaceSnapshot current = workspace.snapshot();
    if (current.documentOpenGeneration == snapshot.documentOpenGeneration
        && current.filePath == snapshot.filePath) {
        reloadPreviewMediaAfterFileOperation(false);
    }
    if (!aligned) {
        requests->postNotice(
            cancelled ? miacode::NoticeSeverity::Information : miacode::NoticeSeverity::Error,
            title, cancelled ? miacode::localizedText("media_tools.video_processing_canceled") : error);
        return;
    }
    if (!workspaceStillCurrent()) {
        return;
    }

    const QFileInfo videoInfo(videoPath);
    const QString backupName = QStringLiteral("%1_bak.%2")
                                   .arg(videoInfo.completeBaseName(), videoInfo.suffix());
    showMediaOperationCompleteDialog(
        title,
        miacode::localizedText("media_tools.video_aligned_to_track_1_2")
            .arg(videoInfo.fileName())
            .arg(videoAudioOffsetSeconds, 0, 'f', 3)
            .arg(backupName),
        videoPath);
}

void miacode::runtime::MediaJobsHost::runCompressBackgroundVideo(
    const miacode::LocalizedText& title, const QString& videoPath, const QString& backupName)
{
    MC_OP("miacode::runtime::MediaJobsHost::runCompressBackgroundVideo");
    miacode::UiRequestService* const requests = session_.uiRequestService();
    if (requests == nullptr) {
        return;
    }
    const QString ffmpegPath = resolveMediaToolFfmpegExecutable();
    if (ffmpegPath.isEmpty()) {
        requests->postNotice(
            miacode::NoticeSeverity::Error, title,
            miacode::localizedText("media_tools.ffmpeg_was_not_found_place"));
        return;
    }

    releasePreviewMediaForFileOperation();

    miacode::LocalizedText error;
    bool cancelled = false;
    bool preservedCompressed = false;
    if (!compressVideoUnder20Mb(
            ffmpegPath, videoPath, session_.jobProgressService(), &error, &cancelled,
            &preservedCompressed)) {
        // preservedCompressed means the encode succeeded and only the
        // auto-replace was blocked by a file lock — a heads-up, not a failure.
        requests->postNotice(
            (cancelled || preservedCompressed) ? miacode::NoticeSeverity::Information
                                               : miacode::NoticeSeverity::Error,
            title,
            cancelled ? miacode::localizedText("media_tools.video_compression_canceled")
                      : error);
        reloadPreviewMediaAfterFileOperation(false);
        return;
    }
    reloadPreviewMediaAfterFileOperation(false);
    showMediaOperationCompleteDialog(
        title,
        miacode::localizedText("media_tools.compressed_1_under_20_mib_2")
            .arg(QFileInfo(videoPath).fileName(), backupName),
        videoPath);
}

void miacode::runtime::MediaJobsHost::onConvertTrackTo44100Hz()
{
    MC_OP("miacode::runtime::MediaJobsHost::onConvertTrackTo44100Hz");
    miacode::UiRequestService* const requests = session_.uiRequestService();
    if (requests == nullptr) {
        return;
    }
    const miacode::LocalizedText title = miacode::localizedText("media_tools.sample_rate");
    const QString chartDirPath = resolveCurrentChartDirectory();
    if (chartDirPath.isEmpty()) {
        requests->postNotice(
            miacode::NoticeSeverity::Warning, title,
            miacode::localizedText("media_tools.open_or_save_a_chart"));
        return;
    }

    const QString trackPath = QDir(chartDirPath).filePath(QStringLiteral("track.mp3"));
    if (!QFileInfo::exists(trackPath)) {
        requests->postNotice(
            miacode::NoticeSeverity::Warning, title,
            miacode::localizedText("media_tools.track_mp3_was_not_found"));
        return;
    }

    requests->requestConfirmation(
        title,
        miacode::localizedText("media_tools.convert_track_mp3_to_44100"),
        miacode::localizedText("media_tools.sample_rate"),
        [this, title, trackPath](bool accepted) {
            if (accepted) {
                runConvertTrackTo44100Hz(title, trackPath);
            }
        });
}

void miacode::runtime::MediaJobsHost::runConvertTrackTo44100Hz(
    const miacode::LocalizedText& title, const QString& trackPath)
{
    MC_OP("miacode::runtime::MediaJobsHost::runConvertTrackTo44100Hz");
    miacode::UiRequestService* const requests = session_.uiRequestService();
    if (requests == nullptr) {
        return;
    }
    const QString ffmpegPath = resolveMediaToolFfmpegExecutable();
    if (ffmpegPath.isEmpty()) {
        requests->postNotice(
            miacode::NoticeSeverity::Error, title,
            miacode::localizedText("media_tools.ffmpeg_was_not_found_place"));
        return;
    }

    releasePreviewMediaForFileOperation();

    miacode::LocalizedText error;
    bool cancelled = false;
    // Qualified: Session now also has a convertTrackTo44100Hz() — the
    // MediaToolsEngine entry point — which would otherwise shadow this
    // file-local ffmpeg helper from inside the nested section class.
    if (!::convertTrackTo44100Hz(ffmpegPath, trackPath, session_.jobProgressService(), &error, &cancelled)) {
        requests->postNotice(
            cancelled ? miacode::NoticeSeverity::Information
                      : miacode::NoticeSeverity::Error,
            title,
            cancelled
                ? miacode::localizedText("media_tools.sample_rate_conversion_canceled")
                : error);
        reloadPreviewMediaAfterFileOperation(true);
        return;
    }
    reloadPreviewMediaAfterFileOperation(true);
    showMediaOperationCompleteDialog(
        title,
        miacode::localizedText("media_tools.converted_track_mp3_to_44100_2"),
        trackPath);
}

miacode::runtime::MediaJobsHost::MediaBlankPaths miacode::runtime::MediaJobsHost::resolveMediaBlankPaths(MediaBlankTarget target) const
{
    MediaBlankPaths paths;
    paths.isTrack = target == MediaBlankTarget::Track;
    paths.title = paths.isTrack
        ? miacode::localizedText("media_tools.prepend_track_silence")
        : miacode::localizedText("media_tools.prepend_pv_black_screen");
    const QString chartDirPath = resolveCurrentChartDirectory();
    if (chartDirPath.isEmpty()) {
        return paths;
    }
    paths.inputPath = paths.isTrack
        ? miacode::chart_assets::resolveTrackPathForDirectory(chartDirPath)
        : miacode::chart_assets::resolveChartVideoPath(
              session_.currentFilePath_, session_.applicationServices_.workspace().document().videoPath);
    const QFileInfo inputInfo(paths.inputPath);
    paths.inputName = inputInfo.fileName();
    paths.backupName = QStringLiteral("%1_bak.%2").arg(inputInfo.completeBaseName(), inputInfo.suffix());
    paths.backupPath = paths.inputPath.isEmpty()
        ? QString()
        : inputInfo.dir().filePath(paths.backupName);
    return paths;
}

QVariantMap miacode::runtime::MediaJobsHost::prependMediaBlankContext(MediaBlankTarget target)
{
    MC_OP("miacode::runtime::MediaJobsHost::prependMediaBlankContext");
    QVariantMap context;
    context.insert(QStringLiteral("available"), false);
    miacode::UiRequestService* const requests = session_.uiRequestService();
    if (requests == nullptr) {
        return context;
    }
    const MediaBlankPaths paths = resolveMediaBlankPaths(target);
    context.insert(QStringLiteral("title"), paths.title.text());
    context.insert(QStringLiteral("isTrack"), paths.isTrack);
    if (paths.inputPath.isEmpty()) {
        requests->postNotice(
            miacode::NoticeSeverity::Warning, paths.title,
            miacode::localizedText("media_tools.open_or_save_a_chart"));
        return context;
    }
    if (!QFileInfo::exists(paths.inputPath)) {
        requests->postNotice(
            miacode::NoticeSeverity::Warning, paths.title,
            miacode::localizedText("media_tools.1_was_not_found_next")
                .arg(paths.isTrack
                         ? miacode::LocalizedText(paths.inputName)
                         : miacode::localizedText("media_tools.background_mp4_video")));
        return context;
    }

    const QVector<SimaiRawField>& extraFields = session_.applicationServices_.workspace().document().extraFields;
    const int clockCount = mediaBlankClockCountFromFields(extraFields);
    const double wholeBpm = miacode::chart_clock::wholeBpmFromFields(extraFields);
    const double chartBpm = miacode::chart_clock::firstBpmFromChart(session_.activeChartText());

    context.insert(QStringLiteral("available"), true);
    context.insert(QStringLiteral("inputName"), paths.inputName);
    context.insert(QStringLiteral("backupName"), paths.backupName);
    context.insert(QStringLiteral("hasBackup"), QFileInfo::exists(paths.backupPath));
    context.insert(QStringLiteral("beats"), clockCount > 0 ? clockCount : 4);
    context.insert(
        QStringLiteral("bpm"),
        wholeBpm > 0.0 ? wholeBpm
                       : (chartBpm > 0.0 ? chartBpm : miacode::chart_clock::kFallbackClockBpm));
    return context;
}

QVariantMap miacode::runtime::MediaJobsHost::detectMediaBlankTiming(MediaBlankTarget target)
{
    // Analysis reads the chart's own track regardless of which media the blank
    // is going in front of: the beat grid comes from the audio either way.
    Q_UNUSED(target);
    QVariantMap detected;
    const QString trackPath = miacode::chart_assets::resolveTrackPath(session_.currentFilePath_);
    const auto decoded = miacode::latency_analysis::decodeMonoTrack(trackPath);
    if (decoded.samples.isEmpty()) {
        return detected;
    }
    const auto envelope =
        miacode::latency_analysis::buildOnsetEnvelope(decoded.samples, decoded.sampleRate);
    const auto result = miacode::latency_analysis::detectBpm(envelope);
    if (!(result.bpm > 0.0)) {
        return detected;
    }
    detected.insert(QStringLiteral("bpm"), result.bpm);
    if (!result.meterId.isEmpty()) {
        detected.insert(QStringLiteral("beats"), mediaBlankBeatsFromMeterId(result.meterId));
    }
    return detected;
}

void miacode::runtime::MediaJobsHost::restoreMediaBlankBackup(MediaBlankTarget target)
{
    MC_OP("miacode::runtime::MediaJobsHost::restoreMediaBlankBackup");
    miacode::UiRequestService* const requests = session_.uiRequestService();
    if (requests == nullptr) {
        return;
    }
    const MediaBlankPaths paths = resolveMediaBlankPaths(target);
    releasePreviewMediaForFileOperation();
    miacode::LocalizedText error;
    if (!restoreFileFromBackup(paths.backupPath, paths.inputPath, &error)) {
        requests->postNotice(miacode::NoticeSeverity::Error, paths.title, error);
    } else {
        requests->postNotice(
            miacode::NoticeSeverity::Information, paths.title,
            miacode::localizedText("media_tools.backup_restored"));
    }
    reloadPreviewMediaAfterFileOperation(paths.isTrack);
}

void miacode::runtime::MediaJobsHost::applyMediaBlank(MediaBlankTarget target, double beats, double bpm)
{
    MC_OP("miacode::runtime::MediaJobsHost::applyMediaBlank");
    miacode::UiRequestService* const requests = session_.uiRequestService();
    if (requests == nullptr || !(bpm > 0.0) || !(beats > 0.0)) {
        return;
    }
    const MediaBlankPaths paths = resolveMediaBlankPaths(target);
    const QString ffmpegPath = resolveMediaToolFfmpegExecutable();
    if (ffmpegPath.isEmpty()) {
        requests->postNotice(
            miacode::NoticeSeverity::Error, paths.title,
            miacode::localizedText("media_tools.ffmpeg_was_not_found_place"));
        return;
    }

    const double silenceSeconds = beats * 60.0 / bpm;
    releasePreviewMediaForFileOperation();

    miacode::LocalizedText error;
    bool cancelled = false;
    const bool ok = paths.isTrack
        ? prependTrackSilence(
              ffmpegPath, paths.inputPath, silenceSeconds, session_.jobProgressService(), &error,
              &cancelled)
        : prependPvBlack(
              ffmpegPath, paths.inputPath, silenceSeconds, session_.jobProgressService(), &error,
              &cancelled);
    if (!ok) {
        requests->postNotice(
            cancelled ? miacode::NoticeSeverity::Information
                      : miacode::NoticeSeverity::Error,
            paths.title,
            cancelled
                ? miacode::localizedText(paths.isTrack ? "media_tools.track_mp3_processing_canceled" : "media_tools.video_processing_canceled")
                : error);
        reloadPreviewMediaAfterFileOperation(paths.isTrack);
        return;
    }

    reloadPreviewMediaAfterFileOperation(paths.isTrack);
    showMediaOperationCompleteDialog(
        paths.title,
        miacode::localizedText("media_tools.prepended_2_s_of_3")
            .arg(paths.inputName)
            .arg(silenceSeconds, 0, 'f', 3)
            .arg(paths.isTrack ? miacode::localizedText("media_tools.silence")
                               : miacode::localizedText("media_tools.black_screen"))
            .arg(paths.backupName),
        paths.inputPath);
}
