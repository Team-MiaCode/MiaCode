#include "export/video_export/VideoExportSettings.h"
#include "export/video_export/VideoExportSnapshot.h"

#include <QJsonObject>
#include <QTextStream>

namespace {

bool require(bool condition, const QString& message, QTextStream& err)
{
    if (!condition) {
        err << "FAIL: " << message << Qt::endl;
    }
    return condition;
}

bool nearlyEqual(double lhs, double rhs)
{
    return qAbs(lhs - rhs) <= 1e-9;
}

bool verifyPreferencesAndDifficultyReseed(QTextStream& err)
{
    VideoExportTask edited;
    edited.introSoundFileName = QStringLiteral("custom-start.flac");

    // The intro volume belongs to the audio settings now; the export
    // preferences no longer carry a copy of it.
    QJsonObject preferences;
    miacode::video_export::appendVideoExportPreferences(&preferences, edited);
    bool ok = require(
        !preferences.contains(QStringLiteral("intro_sound_volume")),
        QStringLiteral("shared export preferences do not serialize an intro volume"),
        err);

    VideoExportTask reseeded;
    reseeded.outputPath = QStringLiteral("new-difficulty.mp4");
    reseeded.exportStartSeconds = 3.0;
    miacode::video_export::copyVideoExportUserSettings(edited, &reseeded);
    ok &= require(
        reseeded.introSoundFileName == QStringLiteral("custom-start.flac"),
        QStringLiteral("difficulty reseeding preserves the selected intro sound"),
        err);
    ok &= require(
        reseeded.outputPath == QStringLiteral("new-difficulty.mp4")
            && nearlyEqual(reseeded.exportStartSeconds, 3.0),
        QStringLiteral("difficulty reseeding keeps chart-owned output and range fields"),
        err);
    return ok;
}

bool verifySnapshotAndWorkerRoundTrip(QTextStream& err)
{
    VideoExportSnapshot source;
    source.chartTextUtf8 = QStringLiteral(
        "&title=Intro Sound Spec\n"
        "&artist=MiaCode\n"
        "&first=0\n"
        "&lv_5=12\n"
        "&inote_5=(120){4}1,\n");
    source.difficultyId = 5;
    source.originalChartPath = QStringLiteral("C:/charts/intro-sound/maidata.txt");
    source.outputPath = QStringLiteral("C:/charts/intro-sound/out.mp4");
    source.contentDurationSeconds = 1.0;
    source.intro.enabled = true;
    source.introSoundFileName = QStringLiteral("../custom-start.flac");
    source.audioSettings.introVolume = 0.6;

    const QJsonObject json = source.toJson();
    const QJsonObject intro = json.value(QStringLiteral("intro")).toObject();
    bool ok = require(
        intro.value(QStringLiteral("sound_file")).toString() == QStringLiteral("custom-start.flac")
            && !intro.contains(QStringLiteral("sound_volume")),
        QStringLiteral("snapshot JSON stores the intro sound basename and no separate volume"),
        err);

    VideoExportSnapshot restored;
    QString error;
    const bool restoredOk = VideoExportSnapshot::fromJson(json, &restored, &error);
    ok &= require(
        restoredOk,
        QStringLiteral("snapshot JSON restores successfully: %1").arg(error),
        err);
    ok &= require(
        restored.introSoundFileName == QStringLiteral("custom-start.flac")
            && qAbs(restored.audioSettings.introVolume - 0.6) <= 1e-9,
        QStringLiteral("snapshot parsing restores the intro sound and the audio-settings intro volume"),
        err);

    VideoExportTask workerTask;
    error.clear();
    const bool workerTaskOk = buildVideoExportTaskFromSnapshot(restored, &workerTask, &error);
    ok &= require(
        workerTaskOk,
        QStringLiteral("worker task rebuild succeeds: %1").arg(error),
        err);
    ok &= require(
        workerTask.introSoundFileName == QStringLiteral("custom-start.flac")
            && qAbs(workerTask.audioSettings.introVolume - 0.6) <= 1e-9,
        QStringLiteral("worker task receives the selected intro sound and the intro volume"),
        err);

    QJsonObject clampedJson = json;
    QJsonObject clampedIntro = clampedJson.value(QStringLiteral("intro")).toObject();
    clampedIntro.insert(QStringLiteral("sound_file"), QStringLiteral("../../unsafe.ogg"));
    clampedJson.insert(QStringLiteral("intro"), clampedIntro);
    VideoExportSnapshot clamped;
    error.clear();
    ok &= require(
        VideoExportSnapshot::fromJson(clampedJson, &clamped, &error)
            && clamped.introSoundFileName == QStringLiteral("unsafe.ogg"),
        QStringLiteral("snapshot parsing strips directories from the intro sound file"),
        err);
    return ok;
}

}  // namespace

int main()
{
    QTextStream err(stderr);
    const bool ok = verifyPreferencesAndDifficultyReseed(err)
        && verifySnapshotAndWorkerRoundTrip(err);
    if (ok) {
        QTextStream out(stdout);
        out << "video_export_intro_sound_spec ok" << Qt::endl;
    }
    return ok ? 0 : 1;
}
