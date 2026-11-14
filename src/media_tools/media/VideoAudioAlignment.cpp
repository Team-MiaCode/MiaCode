#include "media_tools/media/VideoAudioAlignment.h"

#include "common/DebugLog.h"
#include "common/DebugOptions.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSize>
#include <QTemporaryDir>
#include <QThread>
#include <QVector>
#include <QtEndian>

#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <limits>
#include <system_error>

namespace miacode::media {
namespace {

constexpr int kAudioSampleRate = 12000;
constexpr int kEnvelopeRate = 50;
constexpr int kWindowSamples = kAudioSampleRate / kEnvelopeRate;
constexpr int kProcessPollMs = 100;
constexpr int kFileRetryAttempts = 40;
constexpr int kFileRetryDelayMs = 50;

struct MediaInfo {
    double durationSeconds = 0.0;
    qint64 videoBitrate = 0;
    qint64 audioBitrate = 0;
    bool hasAudio = false;
};

class AlignmentContext {
public:
    AlignmentContext(
        const VideoAudioAlignmentCancellation& cancellation,
        const VideoAudioAlignmentProgress& progress)
        : cancellation_(cancellation), progress_(progress)
    {
    }

    bool cancelled() const
    {
        return cancellation_ && cancellation_->load(std::memory_order_relaxed);
    }

    VideoAudioAlignmentStage stage() const { return lastStage_; }

    void report(VideoAudioAlignmentStage stage, int percent)
    {
        if (stage == lastStage_ && percent == lastPercent_) {
            return;
        }
        lastStage_ = stage;
        lastPercent_ = percent;
        if (progress_) {
            progress_(stage, percent);
        }
    }

private:
    const VideoAudioAlignmentCancellation& cancellation_;
    const VideoAudioAlignmentProgress& progress_;
    VideoAudioAlignmentStage lastStage_ = VideoAudioAlignmentStage::Probe;
    int lastPercent_ = -2;
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
    const AlignmentContext& context,
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
    *error = process.errorString();
    return false;
}

void appendDiagnosticTail(QString& tail, const QByteArray& bytes)
{
    tail += QString::fromLocal8Bit(bytes);
    if (tail.size() > 8000) {
        tail = tail.right(8000);
    }
}

bool probeMediaInfo(
    const VideoAudioAlignmentRequest& request,
    MediaInfo* info,
    AlignmentContext& context,
    LocalizedText* error)
{
    context.report(VideoAudioAlignmentStage::Probe, -1);
    QElapsedTimer timer;
    timer.start();
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    if (!startProcess(process, request.ffmpegPath, {
            QStringLiteral("-hide_banner"), QStringLiteral("-nostdin"),
            QStringLiteral("-i"), request.videoPath
        }, context, error)) {
        return false;
    }
    QString output;
    while (process.state() != QProcess::NotRunning) {
        process.waitForReadyRead(kProcessPollMs);
        output += QString::fromLocal8Bit(process.readAll());
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
    output += QString::fromLocal8Bit(process.readAll());
    if (context.cancelled()) {
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
    // ffmpeg exits with code 1 for a header-only request; validate the header instead.
    if (process.exitStatus() != QProcess::NormalExit
        || !durationMatch.hasMatch() || !videoMatch.hasMatch() || !dimensionsMatch.hasMatch()) {
        *error = output.trimmed().isEmpty()
            ? localizedText("media_tools.duration_read_failed")
            : LocalizedText(output.trimmed().right(2000));
        return false;
    }
    info->durationSeconds = durationMatch.captured(1).toDouble() * 3600.0
        + durationMatch.captured(2).toDouble() * 60.0
        + durationMatch.captured(3).toDouble();
    const QSize dimensions(dimensionsMatch.captured(1).toInt(), dimensionsMatch.captured(2).toInt());
    if (!(info->durationSeconds > 0.0) || !dimensions.isValid()) {
        *error = localizedText("media_tools.duration_read_failed");
        return false;
    }
    const QRegularExpressionMatch videoBitrateMatch = bitratePattern.match(videoMatch.captured(1));
    if (videoBitrateMatch.hasMatch()) {
        info->videoBitrate = qRound64(videoBitrateMatch.captured(1).toDouble() * 1000.0);
    }
    const QRegularExpressionMatch audioMatch = audioPattern.match(output);
    info->hasAudio = audioMatch.hasMatch();
    const QRegularExpressionMatch audioBitrateMatch = bitratePattern.match(audioMatch.captured(1));
    if (audioBitrateMatch.hasMatch()) {
        info->audioBitrate = qRound64(audioBitrateMatch.captured(1).toDouble() * 1000.0);
    }
    if (debug_options::runtimeDebugOutputEnabled()) {
        debug_log::appendLine(
            debug_log::Channel::Runtime, QStringLiteral("media/audio_alignment"),
            QStringLiteral("probed file=%1 seconds=%2 video_bps=%3 audio_bps=%4 has_audio=%5 elapsed_ms=%6")
                .arg(QFileInfo(request.videoPath).fileName()).arg(info->durationSeconds, 0, 'f', 3)
                .arg(info->videoBitrate).arg(info->audioBitrate).arg(info->hasAudio).arg(timer.elapsed()));
    }
    return true;
}

class EnvelopeBuilder {
public:
    explicit EnvelopeBuilder(double durationHint)
    {
        const double hintedSize = std::ceil(durationHint * kEnvelopeRate);
        if (hintedSize > 0.0 && hintedSize < std::numeric_limits<int>::max()) {
            values_.reserve(static_cast<qsizetype>(hintedSize));
        }
    }

    bool append(const QByteArray& bytes, const AlignmentContext& context)
    {
        qsizetype offset = 0;
        // Pipe reads may split a float, an RMS window, or both.
        if (partialSize_ != 0) {
            while (partialSize_ < static_cast<int>(sizeof(float)) && offset < bytes.size()) {
                partial_[partialSize_++] = bytes.at(offset++);
            }
            if (partialSize_ == static_cast<int>(sizeof(float))) {
                appendSample(partial_.data());
                partialSize_ = 0;
            }
        }
        while (offset + static_cast<qsizetype>(sizeof(float)) <= bytes.size()) {
            appendSample(bytes.constData() + offset);
            offset += sizeof(float);
            if (sampleCount_ % 4096 == 0 && context.cancelled()) {
                return false;
            }
        }
        while (offset < bytes.size()) {
            partial_[partialSize_++] = bytes.at(offset++);
        }
        return !context.cancelled();
    }

    QVector<double> finish()
    {
        if (windowCount_ != 0) {
            finishWindow();
        }
        return std::move(values_);
    }

    qint64 sampleCount() const { return sampleCount_; }

private:
    void appendSample(const char* data)
    {
        const quint32 bits = qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(data));
        float sample;
        std::memcpy(&sample, &bits, sizeof(sample));
        const double value = sample;
        squareSum_ += value * value;
        ++windowCount_;
        ++sampleCount_;
        if (windowCount_ == kWindowSamples) {
            finishWindow();
        }
    }

    void finishWindow()
    {
        const double rms = std::sqrt(squareSum_ / windowCount_);
        values_.append(qMax(0.0, rms - previousRms_ * 0.85));
        previousRms_ = rms;
        squareSum_ = 0.0;
        windowCount_ = 0;
    }

    QVector<double> values_;
    std::array<char, sizeof(float)> partial_{};
    int partialSize_ = 0;
    int windowCount_ = 0;
    qint64 sampleCount_ = 0;
    double squareSum_ = 0.0;
    double previousRms_ = 0.0;
};

bool decodeEnvelope(
    const QString& ffmpegPath,
    const QString& mediaPath,
    double durationHint,
    QVector<double>* envelope,
    AlignmentContext& context,
    LocalizedText* error)
{
    context.report(VideoAudioAlignmentStage::Decode, -1);
    if (context.cancelled()) {
        return false;
    }
    QElapsedTimer timer;
    timer.start();
    QProcess process;
    process.setProcessChannelMode(QProcess::SeparateChannels);
    if (!startProcess(process, ffmpegPath, {
            QStringLiteral("-hide_banner"), QStringLiteral("-nostdin"),
            QStringLiteral("-loglevel"), QStringLiteral("error"),
            QStringLiteral("-i"), mediaPath,
            QStringLiteral("-map"), QStringLiteral("0:a:0"),
            QStringLiteral("-vn"), QStringLiteral("-sn"), QStringLiteral("-dn"),
            QStringLiteral("-ac"), QStringLiteral("1"),
            QStringLiteral("-ar"), QString::number(kAudioSampleRate),
            QStringLiteral("-f"), QStringLiteral("f32le"), QStringLiteral("pipe:1")
        }, context, error)) {
        return false;
    }
    EnvelopeBuilder builder(durationHint);
    QString stderrTail;
    const auto pump = [&]() {
        process.setReadChannel(QProcess::StandardOutput);
        while (process.bytesAvailable() > 0) {
            if (!builder.append(process.read(64 * 1024), context)) {
                return false;
            }
        }
        appendDiagnosticTail(stderrTail, process.readAllStandardError());
        return !context.cancelled();
    };
    while (process.state() != QProcess::NotRunning) {
        process.waitForReadyRead(kProcessPollMs);
        if (!pump()) {
            stopProcess(process);
            return false;
        }
    }
    if (!pump()) {
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (!stderrTail.trimmed().isEmpty()) {
            *error = LocalizedText(stderrTail.trimmed().right(2000));
        }
        return false;
    }
    if (builder.sampleCount() < kAudioSampleRate) {
        return false;
    }
    *envelope = builder.finish();
    if (debug_options::runtimeDebugOutputEnabled()) {
        debug_log::appendLine(
            debug_log::Channel::Runtime, QStringLiteral("media/audio_alignment"),
            QStringLiteral("decoded_envelope file=%1 seconds=%2 envelope_samples=%3 elapsed_ms=%4")
                .arg(QFileInfo(mediaPath).fileName())
                .arg(static_cast<double>(builder.sampleCount()) / kAudioSampleRate, 0, 'f', 3)
                .arg(envelope->size()).arg(timer.elapsed()));
    }
    return true;
}

bool estimateOffset(
    const QVector<double>& trackEnvelope,
    const QVector<double>& videoEnvelope,
    double* offsetSeconds,
    AlignmentContext& context)
{
    context.report(VideoAudioAlignmentStage::Analyze, 0);
    if (context.cancelled() || trackEnvelope.size() < kEnvelopeRate
        || videoEnvelope.size() < kEnvelopeRate
        || trackEnvelope.size() >= std::numeric_limits<int>::max()
        || videoEnvelope.size() >= std::numeric_limits<int>::max()) {
        return false;
    }
    QElapsedTimer timer;
    timer.start();
    const int trackSize = static_cast<int>(trackEnvelope.size());
    const int videoSize = static_cast<int>(videoEnvelope.size());
    const int minimumOverlap = qMin(3 * kEnvelopeRate,
        qMax(kEnvelopeRate, qMin(trackSize, videoSize) / 2));
    const int maxLag = qMin(15 * kEnvelopeRate, qMax(trackSize, videoSize) - minimumOverlap);
    QVector<double> trackSums(trackSize + 1, 0.0);
    QVector<double> trackSquareSums(trackSize + 1, 0.0);
    QVector<double> videoSums(videoSize + 1, 0.0);
    QVector<double> videoSquareSums(videoSize + 1, 0.0);
    for (int index = 0; index < trackSize; ++index) {
        const double value = trackEnvelope.at(index);
        trackSums[index + 1] = trackSums.at(index) + value;
        trackSquareSums[index + 1] = trackSquareSums.at(index) + value * value;
        if (index % 4096 == 0 && context.cancelled()) {
            return false;
        }
    }
    for (int index = 0; index < videoSize; ++index) {
        const double value = videoEnvelope.at(index);
        videoSums[index + 1] = videoSums.at(index) + value;
        videoSquareSums[index + 1] = videoSquareSums.at(index) + value * value;
        if (index % 4096 == 0 && context.cancelled()) {
            return false;
        }
    }
    QVector<double> scores(maxLag * 2 + 1, -1.0);
    const double* const trackValues = trackEnvelope.constData();
    const double* const videoValues = videoEnvelope.constData();
    double bestScore = -1.0;
    int bestLag = 0;
    int bestOverlap = 0;
    for (int lag = -maxLag; lag <= maxLag; ++lag) {
        context.report(VideoAudioAlignmentStage::Analyze,
            (lag + maxLag) * 100 / (maxLag * 2 + 1));
        if (context.cancelled()) {
            return false;
        }
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
            if (context.cancelled()) {
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
    if (context.cancelled()) {
        return false;
    }
    if (bestScore < 0.35) {
        if (debug_options::runtimeDebugOutputEnabled()) {
            debug_log::appendLine(
                debug_log::Channel::Runtime, QStringLiteral("media/audio_alignment"),
                QStringLiteral("match_rejected score=%1 overlap_seconds=%2 elapsed_ms=%3")
                    .arg(bestScore, 0, 'f', 4)
                    .arg(static_cast<double>(bestOverlap) / kEnvelopeRate).arg(timer.elapsed()));
        }
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
    *offsetSeconds = refinedLag / kEnvelopeRate;
    context.report(VideoAudioAlignmentStage::Analyze, 100);
    if (debug_options::runtimeDebugOutputEnabled()) {
        debug_log::appendLine(
            debug_log::Channel::Runtime, QStringLiteral("media/audio_alignment"),
            QStringLiteral("matched offset_seconds=%1 score=%2 overlap_seconds=%3 max_lag_seconds=%4 elapsed_ms=%5")
                .arg(*offsetSeconds, 0, 'f', 6).arg(bestScore, 0, 'f', 4)
                .arg(static_cast<double>(bestOverlap) / kEnvelopeRate)
                .arg(static_cast<double>(maxLag) / kEnvelopeRate).arg(timer.elapsed()));
    }
    return !context.cancelled();
}

bool encodeAlignedVideo(
    const VideoAudioAlignmentRequest& request,
    const MediaInfo& info,
    double offsetSeconds,
    const QString& outputPath,
    AlignmentContext& context,
    LocalizedText* error)
{
    context.report(VideoAudioAlignmentStage::Encode, 0);
    const qint64 inputBytes = QFileInfo(request.videoPath).size();
    const qint64 payloadBitrate = static_cast<qint64>(
        static_cast<double>(inputBytes) * 8.0 / info.durationSeconds * 0.98);
    qint64 audioBitrate = info.audioBitrate;
    if (audioBitrate <= 0) {
        const qint64 availableAudioBitrate = info.videoBitrate > 0
            ? payloadBitrate - info.videoBitrate : payloadBitrate / 10;
        audioBitrate = qBound(qint64{16000}, availableAudioBitrate, qint64{128000});
    }
    const qint64 videoBitrate = info.videoBitrate > 0
        ? info.videoBitrate : qMax<qint64>(32000, payloadBitrate - audioBitrate);
    if (debug_options::runtimeDebugOutputEnabled()) {
        debug_log::appendLine(
            debug_log::Channel::Runtime, QStringLiteral("media/audio_alignment"),
            QStringLiteral("encoding file=%1 offset_seconds=%2 source_video_bps=%3 source_audio_bps=%4 target_video_bps=%5 target_audio_bps=%6 input_bytes=%7")
                .arg(QFileInfo(request.videoPath).fileName()).arg(offsetSeconds, 0, 'f', 3)
                .arg(info.videoBitrate).arg(info.audioBitrate)
                .arg(videoBitrate).arg(audioBitrate).arg(inputBytes));
    }
    const double adjustmentSeconds = qAbs(offsetSeconds);
    const QString adjustment = QString::number(adjustmentSeconds, 'f', 6);
    QString videoFilter;
    QString audioFilter;
    if (offsetSeconds > 0.0) {
        videoFilter = QStringLiteral("trim=start=%1,setpts=PTS-%1/TB,scale=trunc(iw/2)*2:trunc(ih/2)*2,format=yuv420p")
            .arg(adjustment);
        audioFilter = QStringLiteral("atrim=start=%1,asetpts=PTS-%1/TB").arg(adjustment);
    } else {
        videoFilter = QStringLiteral("tpad=start_mode=add:start_duration=%1:color=black,scale=trunc(iw/2)*2:trunc(ih/2)*2,format=yuv420p")
            .arg(adjustment);
        audioFilter = QStringLiteral("adelay=%1:all=1")
            .arg(QString::number(qRound64(adjustmentSeconds * 1000.0)));
    }
    QProcess process;
    process.setProcessChannelMode(QProcess::SeparateChannels);
    if (!startProcess(process, request.ffmpegPath, {
            QStringLiteral("-progress"), QStringLiteral("pipe:1"), QStringLiteral("-nostats"),
            QStringLiteral("-hide_banner"), QStringLiteral("-nostdin"), QStringLiteral("-y"),
            QStringLiteral("-i"), request.videoPath,
            QStringLiteral("-filter_complex"),
            QStringLiteral("[0:v:0]%1[v];[0:a:0]%2[a]").arg(videoFilter, audioFilter),
            QStringLiteral("-map"), QStringLiteral("[v]"),
            QStringLiteral("-map"), QStringLiteral("[a]"),
            QStringLiteral("-c:v"), QStringLiteral("libx264"),
            QStringLiteral("-preset"), QStringLiteral("veryfast"),
            QStringLiteral("-b:v"), QString::number(videoBitrate),
            QStringLiteral("-pix_fmt"), QStringLiteral("yuv420p"),
            QStringLiteral("-c:a"), QStringLiteral("aac"),
            QStringLiteral("-b:a"), QString::number(audioBitrate),
            QStringLiteral("-movflags"), QStringLiteral("+faststart"), outputPath
        }, context, error)) {
        return false;
    }
    const double durationSeconds = qMax(0.0, info.durationSeconds - offsetSeconds);
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
            if (lastMicros >= 0 && durationSeconds > 0.0) {
                context.report(VideoAudioAlignmentStage::Encode,
                    qBound(0, qRound(static_cast<double>(lastMicros) / 1000000.0 / durationSeconds * 100.0), 99));
            }
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
    if (debug_options::runtimeDebugOutputEnabled()) {
        debug_log::appendLine(
            debug_log::Channel::Runtime, QStringLiteral("media/audio_alignment"),
            QStringLiteral("encoded file=%1 input_bytes=%2 output_bytes=%3 offset_seconds=%4")
                .arg(QFileInfo(request.videoPath).fileName()).arg(inputBytes)
                .arg(QFileInfo(outputPath).size()).arg(offsetSeconds, 0, 'f', 6));
    }
    return true;
}

bool copyBackup(
    const QString& inputPath,
    const QString& temporaryPath,
    AlignmentContext& context,
    LocalizedText* error)
{
    context.report(VideoAudioAlignmentStage::Finalize, 0);
    if (context.cancelled()) {
        return false;
    }
    QFile input(inputPath);
    QFile output(temporaryPath);
    if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
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
            const qint64 written = output.write(bytes.constData() + writtenBytes, bytes.size() - writtenBytes);
            if (written <= 0) {
                *error = output.errorString();
                return false;
            }
            writtenBytes += written;
        }
        copiedBytes += bytes.size();
        if (totalBytes > 0) {
            context.report(VideoAudioAlignmentStage::Finalize,
                qBound(1, static_cast<int>(static_cast<double>(copiedBytes) / totalBytes * 98.0), 98));
        }
    }
    if (!output.flush()) {
        *error = output.errorString();
        return false;
    }
    return !context.cancelled();
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
        // All transaction paths share a directory tree; never fall back to a full file copy.
        std::filesystem::rename(source, destination, error);
        if (!error) {
            return true;
        }
        QThread::msleep(kFileRetryDelayMs);
    }
    debug_log::appendLine(
        debug_log::Channel::Runtime, QStringLiteral("media/audio_alignment"),
        QStringLiteral("rename_failed source=%1 destination=%2 attempts=%3 error_category=%4 error_code=%5 error=%6")
            .arg(sourcePath, destinationPath).arg(attempts)
            .arg(QString::fromLatin1(error.category().name())).arg(error.value())
            .arg(QString::fromLocal8Bit(error.message().c_str())),
        /*force=*/true, debug_log::Level::Error);
    return false;
}

VideoAudioAlignmentStatus commitFiles(
    const QString& videoPath,
    const QString& backupPath,
    const QString& alignedPath,
    const QString& newBackupPath,
    QTemporaryDir& temporaryDirectory,
    AlignmentContext& context,
    LocalizedText* error)
{
    context.report(VideoAudioAlignmentStage::Finalize, 99);
    if (context.cancelled()) {
        return VideoAudioAlignmentStatus::Cancelled;
    }
    const QString originalPath = temporaryDirectory.filePath(QStringLiteral("original.mp4"));
    const QString previousBackupPath = temporaryDirectory.filePath(QStringLiteral("previous_backup.mp4"));
    const bool hadPreviousBackup = QFileInfo::exists(backupPath);
    if (hadPreviousBackup && !QFileInfo(backupPath).isFile()) {
        *error = localizedText("media_tools.failed_to_write_file_1").arg(backupPath);
        return VideoAudioAlignmentStatus::Failed;
    }
    if (hadPreviousBackup && !renameWithRetry(backupPath, previousBackupPath)) {
        *error = localizedText("media_tools.failed_to_write_file_1").arg(backupPath);
        return VideoAudioAlignmentStatus::Failed;
    }
    const auto preserveRecoveryFiles = [&]() {
        temporaryDirectory.setAutoRemove(false);
        *error = *error + LocalizedText(QStringLiteral("\n"))
            + localizedText("media_tools.recovery_files_1").arg(temporaryDirectory.path());
    };
    const auto restorePreviousBackup = [&]() {
        if (hadPreviousBackup && !renameWithRetry(previousBackupPath, backupPath)) {
            preserveRecoveryFiles();
        }
    };
    // This short transaction finishes or rolls back as a unit, even if cancellation arrives.
    if (!renameWithRetry(videoPath, originalPath)) {
        *error = localizedText("media_tools.failed_to_stage_original_file").arg(videoPath);
        restorePreviousBackup();
        return VideoAudioAlignmentStatus::Failed;
    }
    if (!renameWithRetry(alignedPath, videoPath)) {
        *error = localizedText("media_tools.replace_failed").arg(videoPath);
        if (!renameWithRetry(originalPath, videoPath)) {
            preserveRecoveryFiles();
        }
        restorePreviousBackup();
        return VideoAudioAlignmentStatus::Failed;
    }
    if (!renameWithRetry(newBackupPath, backupPath)) {
        *error = localizedText("media_tools.failed_to_write_file_1").arg(backupPath);
        const bool movedOutput = renameWithRetry(videoPath, alignedPath);
        if (!movedOutput || !renameWithRetry(originalPath, videoPath)) {
            preserveRecoveryFiles();
        }
        restorePreviousBackup();
        return VideoAudioAlignmentStatus::Failed;
    }
    context.report(VideoAudioAlignmentStage::Finalize, 100);
    return VideoAudioAlignmentStatus::Aligned;
}

} // namespace

VideoAudioAlignmentResult runVideoAudioAlignment(
    const VideoAudioAlignmentRequest& request,
    const VideoAudioAlignmentCancellation& cancellation,
    const VideoAudioAlignmentProgress& progress)
{
    AlignmentContext context(cancellation, progress);
    VideoAudioAlignmentResult result;
    const auto failedResult = [&]() {
        result.stage = context.stage();
        if (context.cancelled()) {
            result.status = VideoAudioAlignmentStatus::Cancelled;
        }
        return result;
    };
    MediaInfo info;
    if (!probeMediaInfo(request, &info, context, &result.error)) {
        return failedResult();
    }
    if (!info.hasAudio) {
        result.error = localizedText("media_tools.video_audio_missing");
        return failedResult();
    }
    result.error = localizedText("media_tools.video_alignment_offset_failed");
    {
        QVector<double> videoEnvelope;
        QVector<double> trackEnvelope;
        if (!decodeEnvelope(request.ffmpegPath, request.videoPath, info.durationSeconds,
                &videoEnvelope, context, &result.error)
            || !decodeEnvelope(request.ffmpegPath, request.trackPath, info.durationSeconds,
                &trackEnvelope, context, &result.error)
            || !estimateOffset(trackEnvelope, videoEnvelope, &result.offsetSeconds, context)) {
            return failedResult();
        }
    }
    if (context.cancelled()) {
        return failedResult();
    }
    if (qAbs(result.offsetSeconds) < 1.0 / kEnvelopeRate) {
        result.status = VideoAudioAlignmentStatus::AlreadyAligned;
        result.stage = context.stage();
        result.error.clear();
        return result;
    }
    context.report(VideoAudioAlignmentStage::Encode, 0);
    const QFileInfo videoInfo(request.videoPath);
    QTemporaryDir temporaryDirectory(videoInfo.dir().filePath(QStringLiteral(".miacode_pv_align_XXXXXX")));
    if (!temporaryDirectory.isValid()) {
        result.error = localizedText("media_tools.failed_to_write_file_1").arg(videoInfo.absolutePath());
        return failedResult();
    }
    const QString alignedPath = temporaryDirectory.filePath(QStringLiteral("aligned.mp4"));
    const QString newBackupPath = temporaryDirectory.filePath(QStringLiteral("backup.mp4"));
    const QString backupPath = videoInfo.dir().filePath(
        QStringLiteral("%1_bak.%2").arg(videoInfo.completeBaseName(), videoInfo.suffix()));
    if (!encodeAlignedVideo(request, info, result.offsetSeconds, alignedPath, context, &result.error)
        || !copyBackup(request.videoPath, newBackupPath, context, &result.error)) {
        return failedResult();
    }
    result.status = commitFiles(request.videoPath, backupPath, alignedPath, newBackupPath,
        temporaryDirectory, context, &result.error);
    result.stage = context.stage();
    if (result.status == VideoAudioAlignmentStatus::Aligned) {
        result.error.clear();
    }
    return result;
}

} // namespace miacode::media
