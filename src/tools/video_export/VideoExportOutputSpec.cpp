#include "export/video_export/VideoExportController.h"
#include "export/video_export/VideoExportOutput.h"
#include "export/video_export/VideoExportSnapshot.h"
#include "export/video_export/VideoExportSettings.h"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

int main(int argc, char** argv)
{
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
    snapshot.contentDurationSeconds = 1.0;
    VideoExportSnapshot restored;
    QString detail;
    require(VideoExportSnapshot::fromJson(snapshot.toJson(), &restored, &detail)
                && restored.outputMode == snapshot.outputMode, "output mode survives worker snapshots");
    VideoExportTask task;
    task.outputMode = VideoExportOutputMode::Wav;
    VideoExportTask copied;
    miacode::video_export::copyVideoExportUserSettings(task, &copied);
    require(copied.outputMode == task.outputMode, "difficulty changes retain the output mode");

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
