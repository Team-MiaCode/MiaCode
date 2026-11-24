#include "media_tools/media/MediaPrependPolicy.h"


#include <QProcess>

#include <algorithm>
#include <cmath>

namespace miacode::media {
namespace {

QString fixedSeconds(double seconds)
{
    return QString::number(std::max(0.0, seconds), 'f', 6);
}

}  // namespace

QStringList audioEncoderArguments(const QString& suffix, bool highPrecision)
{
    const QString extension = suffix.toLower();
    if (extension == QStringLiteral("mp3")) {
        return {QStringLiteral("-c:a"), QStringLiteral("libmp3lame"),
                QStringLiteral("-q:a"), QStringLiteral("2")};
    }
    if (extension == QStringLiteral("wav")) {
        if (highPrecision) {
            return {QStringLiteral("-c:a"), QStringLiteral("pcm_s24le"),
                    QStringLiteral("-rf64"), QStringLiteral("auto")};
        }
        return {QStringLiteral("-c:a"), QStringLiteral("pcm_s16le")};
    }
    if (extension == QStringLiteral("flac")) {
        if (highPrecision) {
            return {QStringLiteral("-c:a"), QStringLiteral("flac"),
                    QStringLiteral("-sample_fmt"), QStringLiteral("s32")};
        }
        return {QStringLiteral("-c:a"), QStringLiteral("flac")};
    }
    if (extension == QStringLiteral("ogg")) {
        return {QStringLiteral("-c:a"), QStringLiteral("libvorbis"),
                QStringLiteral("-q:a"), QStringLiteral("6")};
    }
    return {};
}

QStringList makeAudioPrependArguments(
    const QString& inputPath,
    const QString& outputPath,
    const QString& outputSuffix,
    double silenceSeconds)
{
    const QString duration = fixedSeconds(silenceSeconds);
    const QString audioFormat = QStringLiteral(
        "aformat=sample_fmts=fltp:sample_rates=44100:channel_layouts=stereo");
    QStringList args{
        QStringLiteral("-hide_banner"),
        QStringLiteral("-nostdin"),
        QStringLiteral("-y"),
        QStringLiteral("-f"), QStringLiteral("lavfi"),
        QStringLiteral("-i"),
        QStringLiteral("anullsrc=channel_layout=stereo:sample_rate=44100:d=%1").arg(duration),
        QStringLiteral("-i"), inputPath,
        QStringLiteral("-filter_complex"),
        QStringLiteral(
            "[0:a]atrim=duration=%1,%2,asetpts=PTS-STARTPTS[s];"
            "[1:a]aresample=44100,%2,asetpts=PTS-STARTPTS[a];"
            "[s][a]concat=n=2:v=0:a=1[out]")
            .arg(duration, audioFormat),
        QStringLiteral("-map"), QStringLiteral("[out]"),
    };
    args << audioEncoderArguments(outputSuffix);
    args << outputPath;
    return args;
}

PvCompressionPlan makePvPrependCompressionPlan(
    qint64 originalBytes,
    double inputDurationSeconds,
    double blackSeconds)
{
    const double totalDurationSeconds = inputDurationSeconds + std::max(0.0, blackSeconds);
    qint64 targetBytes = kPvCompressionWorkingTargetBytes;
    if (originalBytes > 0 && inputDurationSeconds > 0.0) {
        const double proportionalBytes = static_cast<double>(originalBytes)
            * totalDurationSeconds / inputDurationSeconds
            * kPvPrependContainerAllowanceRatio;
        targetBytes = static_cast<qint64>(std::ceil(proportionalBytes));
    }
    return makePvCompressionPlan(totalDurationSeconds, targetBytes);
}

QStringList makePvPrependPassArguments(
    const QString& inputPath,
    const QString& outputPath,
    const QString& passLogPath,
    const PvCompressionPlan& plan,
    double blackSeconds,
    int passNumber)
{
    QStringList args{
        QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-nostdin"),
        QStringLiteral("-y"),
        QStringLiteral("-i"), inputPath,
        QStringLiteral("-map"), QStringLiteral("0:v:0"),
        QStringLiteral("-map_metadata"), QStringLiteral("-1"),
        QStringLiteral("-c:v"), QStringLiteral("libx264"),
        QStringLiteral("-preset"), QStringLiteral("veryfast"),
        QStringLiteral("-b:v"), QStringLiteral("%1k").arg(plan.videoBitrateKbps),
        QStringLiteral("-vf"),
        QStringLiteral("tpad=start_duration=%1:start_mode=add:color=black,format=yuv420p")
            .arg(fixedSeconds(blackSeconds)),
        QStringLiteral("-pix_fmt"), QStringLiteral("yuv420p"),
        QStringLiteral("-fps_mode"), QStringLiteral("passthrough"),
        QStringLiteral("-an"),
        QStringLiteral("-pass"), QString::number(passNumber),
        QStringLiteral("-passlogfile"), passLogPath,
    };
    if (passNumber == 1) {
        args << QStringLiteral("-f") << QStringLiteral("null") << QProcess::nullDevice();
    } else {
        args << QStringLiteral("-movflags") << QStringLiteral("+faststart") << outputPath;
    }
    return args;
}

bool isAcceptablePvPrependOutput(const PvCompressionPlan& plan, qint64 outputBytes)
{
    const qint64 toleratedTarget = static_cast<qint64>(
        std::ceil(static_cast<double>(plan.targetBytes) * kPvPrependOutputToleranceRatio));
    return outputBytes > 0
        && outputBytes < kPvCompressionHardLimitBytes
        && outputBytes <= toleratedTarget;
}

}  // namespace miacode::media
