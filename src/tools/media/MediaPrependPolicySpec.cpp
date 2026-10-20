#include "media_tools/media/MediaPrependPolicy.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextStream>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

bool run(const QString& executable, const QStringList& arguments, QByteArray* output = nullptr)
{
    QProcess process;
    process.start(executable, arguments);
    if (!process.waitForStarted(5000) || !process.waitForFinished(30000)
        || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        QTextStream(stderr) << process.errorString() << '\n' << process.readAllStandardError() << Qt::endl;
        return false;
    }
    if (output != nullptr) {
        *output = process.readAllStandardOutput();
    }
    return true;
}

bool checkAudio(const QString& ffmpeg, const QString& directory, const QString& suffix)
{
    const QString source = QDir(directory).filePath(QStringLiteral("source.%1").arg(suffix));
    const QString output = QDir(directory).filePath(QStringLiteral("padded.%1").arg(suffix));
    if (!run(ffmpeg, {"-hide_banner", "-loglevel", "error", "-y", "-f", "lavfi", "-i",
                     "sine=frequency=1000:sample_rate=44100:duration=0.2", "-ac", "2", source})
        || !run(ffmpeg, miacode::media::makeAudioPrependArguments(source, output, suffix, 0.15))) {
        return false;
    }
    QByteArray pcm;
    if (!run(ffmpeg, {"-hide_banner", "-loglevel", "error", "-i", output,
                     "-ar", "44100", "-ac", "2", "-f", "f32le", "-"}, &pcm)) {
        return false;
    }
    const double duration = static_cast<double>(pcm.size()) / (44100 * 2 * sizeof(float));
    if (std::abs(duration - 0.35) > 0.06) {
        QTextStream(stderr) << suffix << ": padded duration " << duration << Qt::endl;
        return false;
    }
    for (int offset = 0; offset < 4410 * 2 * static_cast<int>(sizeof(float)); offset += sizeof(float)) {
        float sample = 0.0f;
        std::memcpy(&sample, pcm.constData() + offset, sizeof(sample));
        if (std::abs(sample) > 0.001f) {
            QTextStream(stderr) << suffix << ": initial audio is silence" << Qt::endl;
            return false;
        }
    }
    return true;
}

bool checkVideo(const QString& ffmpeg, const QString& directory)
{
    using namespace miacode::media;
    const QString source = QDir(directory).filePath(QStringLiteral("source.mp4"));
    const QString output = QDir(directory).filePath(QStringLiteral("padded.mp4"));
    const QString passLog = QDir(directory).filePath(QStringLiteral("video-pass"));
    if (!run(ffmpeg, {"-hide_banner", "-loglevel", "error", "-y", "-f", "lavfi", "-i",
                     "testsrc2=size=320x180:rate=24:duration=2", "-c:v", "libx264",
                     "-preset", "veryfast", "-b:v", "1000k", "-pix_fmt", "yuv420p", source})) {
        return false;
    }
    auto plan = makePvPrependCompressionPlan(QFileInfo(source).size(), 2.0, 0.5);
    if (!run(ffmpeg, makePvPrependPassArguments(source, output, passLog, plan, 0.5, 1))
        || !run(ffmpeg, makePvPrependPassArguments(source, output, passLog, plan, 0.5, 2))
        || !isAcceptablePvPrependOutput(plan, QFileInfo(output).size())) {
        return false;
    }
    QByteArray firstFrame;
    if (!run(ffmpeg, {"-hide_banner", "-loglevel", "error", "-i", output,
                     "-frames:v", "1", "-pix_fmt", "gray", "-f", "rawvideo", "-"}, &firstFrame)
        || firstFrame.size() != 320 * 180
        || !std::all_of(firstFrame.begin(), firstFrame.end(), [](char value) {
            return static_cast<unsigned char>(value) <= 4;
        })) {
        QTextStream(stderr) << "prepended PV retains source dimensions and starts with black" << Qt::endl;
        return false;
    }
    QByteArray progress;
    if (!run(ffmpeg, {"-hide_banner", "-loglevel", "error", "-i", output,
                     "-progress", "pipe:1", "-nostats", "-f", "null", "-"}, &progress)) {
        return false;
    }
    int frames = 0;
    qint64 durationUs = 0;
    for (const auto& line : QString::fromUtf8(progress).split('\n')) {
        if (line.startsWith(QStringLiteral("frame="))) frames = line.mid(6).toInt();
        if (line.startsWith(QStringLiteral("out_time_us="))) durationUs = line.mid(12).toLongLong();
    }
    const auto capped = makePvPrependCompressionPlan(19'000'000, 2.0, 10.0);
    return frames == 60 && std::abs(durationUs - 2'500'000) <= 50'000
        && capped.targetBytes == kPvCompressionWorkingTargetBytes
        && !isAcceptablePvPrependOutput(capped, kPvCompressionHardLimitBytes);
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const QString ffmpeg = qEnvironmentVariable("MIACODE_FFMPEG_PATH");
    QTemporaryDir directory;
    if (ffmpeg.isEmpty() || !directory.isValid()) {
        QTextStream(stderr) << "Set MIACODE_FFMPEG_PATH to the export FFmpeg executable." << Qt::endl;
        return 1;
    }
    for (const auto& suffix : {QStringLiteral("mp3"), QStringLiteral("wav"),
                              QStringLiteral("flac"), QStringLiteral("ogg")}) {
        if (!checkAudio(ffmpeg, directory.path(), suffix)) {
            return 1;
        }
    }
    return checkVideo(ffmpeg, directory.path()) ? 0 : 1;
}
