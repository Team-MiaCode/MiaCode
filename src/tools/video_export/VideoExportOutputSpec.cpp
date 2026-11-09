#include "export/video_export/VideoExportController.h"
#include "export/video_export/VideoExportControllerInternal.h"
#include "export/video_export/VideoExportOutput.h"
#include "export/video_export/VideoExportSnapshot.h"
#include "export/video_export/VideoExportSettings.h"

#include <QCoreApplication>
#include <QFile>
#include <QGuiApplication>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>

#include <algorithm>

namespace {

bool verifyEncoderRateParameters()
{
    using namespace miacode::video_export::detail;
    for (const QString& codec : {QStringLiteral("libx264"), QStringLiteral("libopenh264"),
            QStringLiteral("mpeg4"), QStringLiteral("h264_nvenc"), QStringLiteral("h264_qsv"),
            QStringLiteral("h264_mf"), QStringLiteral("h264_vaapi"), QStringLiteral("h264_videotoolbox")}) {
        for (const auto sizePreset : {VideoExportSizePreset::Target30Mb, VideoExportSizePreset::Custom}) {
            VideoEncoderConfig encoder;
            encoder.codec = codec;
            encoder.extraArgs << QStringLiteral("probe-option");
            applyVideoEncodingParameters(&encoder, VideoExportPreset::HighQuality, sizePreset, 640, 360, 30,
                SystemMemoryInfo{}, ExportRuntimeConfig{}, 350);
            const qsizetype bitrate = encoder.extraArgs.indexOf(QStringLiteral("-b:v"));
            const bool bounded = sizePreset == VideoExportSizePreset::Target30Mb && codec != QLatin1String("libopenh264");
            if (encoder.codec != codec || encoder.extraArgs.value(0) != QLatin1String("probe-option")
                || bitrate < 0 || encoder.extraArgs.value(bitrate + 1) != QLatin1String("350k")
                || encoder.extraArgs.contains(QStringLiteral("-crf"))
                || encoder.extraArgs.contains(QStringLiteral("-maxrate")) != bounded) {
                return false;
            }
        }
    }
    return true;
}

int verifyMp4Encoding(int argc, char** argv)
{
    const bool verifyInterruptedCorrection = argc == 4 && QByteArray(argv[3]) == "--staged-correction-fixture";
    QGuiApplication app(argc, argv);
    QTemporaryDir directory;
    QTextStream out(stdout);
    QTextStream error(stderr);
    out << "MP4 fixture directory: " << directory.path() << Qt::endl;
    using miacode::video_export::videoExportSizePresetToken;
    using miacode::video_export::videoExportTargetBytes;
    VideoExportTask task;
    task.skinDirectory = QString::fromLocal8Bit(argv[2]);
    task.outputWidth = 360;
    task.outputHeight = 360;
    task.fps = 30;
    task.contentDurationSeconds = 1.0;
    task.intro.enabled = false;
    task.clockCountEnabled = false;
    task.targetVideoBitrateMbps = 1.5;
    for (const auto preset : {VideoExportSizePreset::Target30Mb, VideoExportSizePreset::Target100Mb,
                             VideoExportSizePreset::Target200Mb, VideoExportSizePreset::Custom}) {
        task.sizePreset = preset;
        const QString token = videoExportSizePresetToken(preset);
        task.outputPath = directory.filePath(token + QStringLiteral(".mp4"));
        const auto result = VideoExportController::exportPreparedTask(task);
        QFile output(task.outputPath);
        const bool hasHeader = output.open(QIODevice::ReadOnly) && output.read(8).mid(4) == "ftyp";
        const qint64 budget = videoExportTargetBytes(preset);
        if (!result.success || !hasHeader || (budget > 0 && output.size() > budget)) {
            error << token << ": " << result.message << '\n' << result.details << Qt::endl;
            return 1;
        }
        out << token << ' ' << output.size() << " bytes" << Qt::endl;
    }
    task.sizePreset = VideoExportSizePreset::Target30Mb;
    if (verifyInterruptedCorrection) {
        for (const QString& scenario : {QStringLiteral("cancel"), QStringLiteral("failure")}) {
            const bool cancel = scenario == QLatin1String("cancel");
            task.outputPath = directory.filePath(QStringLiteral("target_30mb_%1.mp4").arg(scenario));
            QFile previous(task.outputPath);
            const QByteArray original("previous output must survive");
            if (!previous.open(QIODevice::WriteOnly) || previous.write(original) != original.size()) {
                error << "Cannot create the existing-output fixture" << Qt::endl;
                return 1;
            }
            previous.close();
            bool cancelRequested = false;
            const auto interrupted = VideoExportController::exportPreparedTask(task,
                [&](int, const QString& message) {
                    cancelRequested = cancel && message == QLatin1String(miacode::video_export::kVideoExportSizeCorrectionProgress);
                    return cancelRequested;
                });
            const QString expectedMessage = cancel ? QStringLiteral("canceled")
                : QStringLiteral("Unable to meet the selected file size budget.");
            if (interrupted.success || interrupted.message != expectedMessage || (cancel && !cancelRequested)
                || !previous.open(QIODevice::ReadOnly) || previous.readAll() != original) {
                error << "Size correction " << scenario << " did not preserve the existing output: "
                      << interrupted.message << Qt::endl;
                return 1;
            }
            out << "Interrupted size correction preserved existing output: " << scenario << Qt::endl;
        }
    }
    task.contentDurationSeconds = 3600.0;
    task.outputPath = directory.filePath(QStringLiteral("impossible.mp4"));
    const auto impossible = VideoExportController::exportPreparedTask(task);
    if (impossible.success || QFile::exists(task.outputPath) || !impossible.message.contains(QStringLiteral("too small"))) {
        error << "An impossible size budget should fail before writing output: " << impossible.message << Qt::endl;
        return 1;
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv)
{
    QStandardPaths::setTestModeEnabled(true);
    if (argc >= 3 && QByteArray(argv[1]) == "--encode-mp4") {
        return verifyMp4Encoding(argc, argv);
    }
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    QTextStream error(stderr);
    bool ok = true;
    const auto require = [&](bool condition, const char* message) {
        if (!condition) {
            error << message << Qt::endl;
            ok = false;
        }
    };
    require(directory.isValid(), "temporary output directory is available");
    require(verifyEncoderRateParameters(), "selected encoder and its probe options survive explicit bitrate settings");
    const auto& resolutions = miacode::video_export::kVideoExportResolutionPresets;
    const auto containsResolution = [&resolutions](int width, int height) {
        return std::any_of(resolutions.begin(), resolutions.end(), [width, height](const auto& preset) {
            return preset.width == width && preset.height == height;
        });
    };
    require(resolutions.size() == 21 && resolutions[3].width == 1080 && resolutions[3].height == 1080
                && containsResolution(640, 360) && containsResolution(854, 480)
                && containsResolution(960, 540) && containsResolution(3840, 2160)
                && !containsResolution(1024, 1024), "resolution additions retain the existing 1080 square default");
    const QString base = directory.filePath(QStringLiteral("chart.mp4"));
    require(videoExportPrimaryOutputPath(base, VideoExportOutputMode::Wav)
                == directory.filePath(QStringLiteral("chart.wav")), "WAV changes the output suffix");
    require(videoExportPrimaryOutputPath(QString(), VideoExportOutputMode::Wav).isEmpty(),
            "empty paths retain their empty value");
    QFile companion(directory.filePath(QStringLiteral("chart.wav")));
    require(companion.open(QIODevice::WriteOnly), "companion output exists");
    companion.close();
    require(makeUniqueVideoExportOutputPath(base, VideoExportOutputMode::Mp4AndWav)
                == directory.filePath(QStringLiteral("chart(1).mp4")),
            "combined output checks both companion names for collisions");

    VideoExportSnapshot snapshot;
    snapshot.difficultyId = 5;
    snapshot.projectDir = directory.path();
    snapshot.originalChartPath = directory.filePath(QStringLiteral("maidata.txt"));
    snapshot.chartTextUtf8 = QStringLiteral("(120){4},E");
    snapshot.outputPath = base;
    snapshot.outputMode = VideoExportOutputMode::Mp4AndWav;
    snapshot.sizePreset = VideoExportSizePreset::Custom;
    snapshot.targetVideoBitrateMbps = 4.25;
    snapshot.contentDurationSeconds = 1.0;
    VideoExportSnapshot restored;
    QString detail;
    require(VideoExportSnapshot::fromJson(snapshot.toJson(), &restored, &detail)
                && restored.outputMode == snapshot.outputMode && restored.sizePreset == snapshot.sizePreset
                && restored.targetVideoBitrateMbps == 4.25, "output mode and custom bitrate survive worker snapshots");
    QJsonObject legacySnapshot = snapshot.toJson();
    QJsonObject legacyExport = legacySnapshot.value(QStringLiteral("export")).toObject();
    legacyExport.remove(QStringLiteral("target_video_bitrate_mbps"));
    legacySnapshot.insert(QStringLiteral("export"), legacyExport);
    require(VideoExportSnapshot::fromJson(legacySnapshot, &restored, &detail)
                && restored.targetVideoBitrateMbps == 8.0, "older snapshots use the default custom bitrate");
    VideoExportTask task;
    task.outputMode = VideoExportOutputMode::Wav;
    task.sizePreset = VideoExportSizePreset::Custom;
    task.targetVideoBitrateMbps = 11.5;
    VideoExportTask copied;
    miacode::video_export::copyVideoExportUserSettings(task, &copied);
    require(copied.outputMode == task.outputMode && copied.sizePreset == task.sizePreset
                && copied.targetVideoBitrateMbps == 11.5, "difficulty changes retain encoding options");
    QJsonObject preferences;
    miacode::video_export::appendVideoExportPreferences(&preferences, task);
    VideoExportTask preferred;
    miacode::video_export::applyVideoExportPreferences(preferences, &preferred);
    require(preferred.sizePreset == task.sizePreset && preferred.targetVideoBitrateMbps == 11.5,
            "video export preferences restore the selected bitrate mode and value");

    task.outputPath = directory.filePath(QStringLiteral("silence.wav"));
    task.skinDirectory = directory.path();
    task.contentDurationSeconds = 1.0;
    task.clockCountEnabled = false;
    const auto result = VideoExportController::exportPreparedTask(task);
    require(result.success, "WAV-only export succeeds through the offline audio path");
    if (!result.success) {
        error << result.message << '\n' << result.details << Qt::endl;
    }
    QFile output(task.outputPath);
    require(output.open(QIODevice::ReadOnly), "WAV output is published");
    const QByteArray header = output.read(12);
    require(header.startsWith("RIFF") && header.mid(8, 4) == "WAVE", "output has a WAV container header");
    require(output.size() >= 44100 * 2 * 2, "WAV contains the requested second of stereo PCM");
    return ok ? 0 : 1;
}
