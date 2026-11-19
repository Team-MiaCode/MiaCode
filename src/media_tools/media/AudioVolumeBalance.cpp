#include "media_tools/media/AudioVolumeBalance.h"
#include "media_tools/media/MediaPrependPolicy.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTemporaryDir>

#include <algorithm>
#include <cmath>

namespace miacode::media {
namespace {

constexpr double kTruePeak = -1.5;
constexpr double kBoostTarget = -6.0;
constexpr double kMeasuredLoudnessTolerance = 0.10;
constexpr double kLoudBoundary = -5.0;
constexpr double kLoudTarget = -5.5;

struct ProcessResult {
    bool ok;
    QByteArray log;
};

ProcessResult runFfmpeg(const QString& ffmpegPath, const QStringList& arguments,
                       const std::atomic_bool& cancelled)
{
    if (cancelled.load()) {
        return {false, {}};
    }

    QProcess process;
    process.setReadChannel(QProcess::StandardError);
    process.setStandardOutputFile(QProcess::nullDevice());
    process.start(ffmpegPath, arguments, QIODevice::ReadOnly);
    if (!process.waitForStarted()) {
        return {false, process.errorString().toUtf8()};
    }

    QByteArray header;
    QByteArray log;
    const auto readLog = [&] {
        const QByteArray chunk = process.readAllStandardError();
        constexpr qsizetype maxHeaderSize = 8 * 1024;
        if (header.size() < maxHeaderSize) {
            header.append(chunk.first(qMin(chunk.size(), maxHeaderSize - header.size())));
        }
        log.append(chunk);
        constexpr qsizetype maxLogSize = 32 * 1024;
        if (log.size() > maxLogSize) {
            log.remove(0, log.size() - maxLogSize);
        }
    };
    while (process.state() != QProcess::NotRunning) {
        if (cancelled.load()) {
            process.kill();
            process.waitForFinished();
            return {false, {}};
        }
        process.waitForReadyRead(100);
        readLog();
    }
    readLog();
    return {process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0, header + log};
}

struct BalancePlan {
    bool valid = false;
    QString filter;
};

QJsonObject loudnessReport(const QByteArray& log)
{
    const qsizetype end = log.lastIndexOf('}');
    const qsizetype start = log.lastIndexOf('{');
    if (start < 0 || end < start) {
        return {};
    }
    return QJsonDocument::fromJson(log.mid(start, end - start + 1)).object();
}

BalancePlan planVolumeBalance(const QJsonObject& report)
{
    const QString peak = report.value(QStringLiteral("input_tp")).toString();
    const QString loudness = report.value(QStringLiteral("input_i")).toString();
    if (peak == QStringLiteral("-inf") && loudness == QStringLiteral("-inf")) {
        return {true, {}};
    }

    bool peakOk = false;
    const double peakDb = peak.toDouble(&peakOk);
    if (!peakOk || !std::isfinite(peakDb)) {
        return {};
    }
    if (loudness == QStringLiteral("-inf")) {
        // Short clips and signals below the loudness gate need a peak target.
        const double gain = peakDb > kTruePeak
            ? kTruePeak - peakDb : std::clamp(kTruePeak - peakDb, 0.0, 72.0);
        return {true, gain != 0.0 ? QStringLiteral("volume=%1dB").arg(gain) : QString()};
    }

    bool loudnessOk = false;
    const double inputLoudness = loudness.toDouble(&loudnessOk);
    if (!loudnessOk || !std::isfinite(inputLoudness)) {
        return {};
    }
    if (inputLoudness >= kBoostTarget - kMeasuredLoudnessTolerance && inputLoudness <= kLoudBoundary) {
        return {true, {}};
    }

    if (inputLoudness > kLoudBoundary) {
        const double gain = std::min(kLoudTarget - inputLoudness, kTruePeak - peakDb);
        return {true, QStringLiteral("volume=%1dB").arg(gain)};
    }
    // Limit gain to the measured true-peak headroom and preserve dynamics.
    const double gain = std::min(kBoostTarget - inputLoudness, kTruePeak - peakDb);
    return {true, gain >= kMeasuredLoudnessTolerance
        ? QStringLiteral("volume=%1dB").arg(gain) : QString()};
}

ProcessResult analyzeLoudness(const QString& ffmpegPath, const QString& trackPath,
                              const std::atomic_bool& cancelled)
{
    return runFfmpeg(ffmpegPath, {
        QStringLiteral("-hide_banner"), QStringLiteral("-nostdin"), QStringLiteral("-nostats"),
        QStringLiteral("-xerror"), QStringLiteral("-i"), trackPath,
        QStringLiteral("-map"), QStringLiteral("0:a:0"),
        QStringLiteral("-af"), QStringLiteral("loudnorm=print_format=json"),
        QStringLiteral("-f"), QStringLiteral("null"), QStringLiteral("-"),
    }, cancelled);
}

} // namespace

AudioVolumeBalanceResult runAudioVolumeBalance(
    const QString& ffmpegPath, const QString& trackPath, const QString& backupPath,
    const std::atomic_bool& cancelled)
{
    const auto failed = [](const LocalizedText& message, const QByteArray& log = {}) {
        return AudioVolumeBalanceResult{AudioVolumeBalanceStatus::Failed,
                                        message, QString::fromUtf8(log).trimmed()};
    };
    const AudioVolumeBalanceResult cancelledResult{AudioVolumeBalanceStatus::Cancelled, {}, {}};
    const QFileInfo source(trackPath);
    const QString extension = source.suffix().toLower();
    const QStringList encoderArguments = audioEncoderArguments(extension, /*highPrecision=*/true);
    if (encoderArguments.isEmpty() || !source.isFile() || source.isSymLink()) {
        return failed(localizedText("media_tools.volume_balance_analyze_failed"));
    }
    const qint64 originalSize = source.size();
    const QDateTime originalModified = source.lastModified();
    const auto replaceFailed = localizedText("media_tools.replace_failed").arg(source.fileName());
    const QStringList commonArguments{
        QStringLiteral("-hide_banner"), QStringLiteral("-nostdin"), QStringLiteral("-nostats"),
        QStringLiteral("-xerror"), QStringLiteral("-i"), trackPath,
        QStringLiteral("-map"), QStringLiteral("0:a:0"),
    };
    const auto analysis = analyzeLoudness(ffmpegPath, trackPath, cancelled);
    if (cancelled.load()) {
        return cancelledResult;
    }
    const auto plan = planVolumeBalance(loudnessReport(analysis.log));
    if (!analysis.ok || !plan.valid) {
        return failed(localizedText("media_tools.volume_balance_analyze_failed"), analysis.log);
    }
    if (plan.filter.isEmpty()) {
        return {AudioVolumeBalanceStatus::Unchanged, {}, {}};
    }
    // Keep the original sample rate when encoding the adjusted track.
    const QRegularExpression inputRate(QStringLiteral(R"(Stream #.*Audio:[^\r\n]*?([0-9]+) Hz)"));
    const QString sampleRate = inputRate.match(QString::fromUtf8(analysis.log)).captured(1);
    if (sampleRate.isEmpty()) {
        return failed(localizedText("media_tools.volume_balance_analyze_failed"), analysis.log);
    }

    QTemporaryDir outputDirectory(source.dir().filePath(QStringLiteral(".miacode-audio-XXXXXX")));
    if (!outputDirectory.isValid()) {
        return failed(replaceFailed, outputDirectory.errorString().toUtf8());
    }
    const QString temporaryPath = outputDirectory.filePath(QStringLiteral("audio.") + extension);
    const auto encode = runFfmpeg(ffmpegPath, commonArguments + QStringList{
        QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-af"), plan.filter,
        QStringLiteral("-ar"), sampleRate,
    } + encoderArguments + QStringList{temporaryPath}, cancelled);
    if (cancelled.load()) {
        return cancelledResult;
    }
    if (!encode.ok || QFileInfo(temporaryPath).size() <= 0) {
        return failed(replaceFailed, encode.log);
    }
    QFile encoded(temporaryPath);
    QSaveFile replacement(trackPath);
    if (!encoded.open(QIODevice::ReadOnly) || !replacement.open(QIODevice::WriteOnly)) {
        return failed(replaceFailed, replacement.errorString().toUtf8());
    }
    while (!encoded.atEnd()) {
        if (cancelled.load()) {
            return cancelledResult;
        }
        const QByteArray bytes = encoded.read(1024 * 1024);
        if (bytes.isEmpty() || replacement.write(bytes) != bytes.size()) {
            return failed(replaceFailed, replacement.errorString().toUtf8());
        }
    }
    const QFileInfo current(trackPath);
    if (current.size() != originalSize || current.lastModified() != originalModified || current.isSymLink()) {
        return failed(localizedText("media_tools.volume_balance_track_changed"));
    }
    const QFileInfo backup(backupPath);
    if (backup.isSymLink() || (backup.exists() && !backup.isFile())
        || (!backup.exists() && !QFile::copy(trackPath, backupPath))) {
        return failed(localizedText("media_tools.failed_to_write_file_1").arg(backup.fileName()));
    }
    if (cancelled.load()) {
        return cancelledResult;
    }
    if (!replacement.commit()) {
        return failed(replaceFailed, replacement.errorString().toUtf8());
    }
    return {AudioVolumeBalanceStatus::Saved, {}, {}};
}

} // namespace miacode::media
