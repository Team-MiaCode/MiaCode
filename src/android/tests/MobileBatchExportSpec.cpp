#include "android/MobileBatchExport.h"
#include "android/MobileExportTask.h"
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QDataStream>
#include <QFile>
#include <cstdio>

namespace {
int failures = 0;
void check(bool condition, const char* message) {
    if (!condition) { ++failures; std::fprintf(stderr, "FAIL: %s\n", message); }
}
void write(const QString& path, const QByteArray& bytes) { QFile f(path); check(f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size(), "fixture written"); }
QString chart(const QString& root, const QString& name, const QString& first, bool audio = true) {
    const auto directory = QDir(root).filePath(name); QDir().mkpath(directory);
    write(directory + "/maidata.txt", ("&title=Same / title\n&artist=Queue artist\n&first=" + first
        + "\n&des=Global author\n&clock_count=2\n&wholebpm=174.5\n&lv_5=13\n&des_5=Chart author\n&inote_5=(120){4}1,2,E\n&inote_6=(120){4}3,4,E\n").toUtf8());
    if (audio) {
        QByteArray wav; QDataStream stream(&wav, QIODevice::WriteOnly); stream.setByteOrder(QDataStream::LittleEndian);
        stream.writeRawData("RIFF", 4); stream << quint32(36 + 16000); stream.writeRawData("WAVEfmt ", 8);
        stream << quint32(16) << quint16(1) << quint16(1) << quint32(8000) << quint32(16000) << quint16(2) << quint16(16);
        stream.writeRawData("data", 4); stream << quint32(16000); wav.append(QByteArray(16000, '\0'));
        write(directory + "/track.wav", wav);
    }
    return directory;
}
}
int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    QTemporaryDir directory;
    const auto a = chart(directory.path(), "one", "0.25");
    const auto b = chart(directory.path(), "two", "0.25");
    const auto bad = chart(directory.path(), "bad-offset", "nan");
    const auto missing = chart(directory.path(), "no-audio", "0", false);
    const auto output = directory.path() + "/output"; QDir().mkpath(output);
    VideoExportTask settings;
    settings.outputPath = "chosen.wav"; settings.exportStartSeconds = 20; settings.contentDurationSeconds = 1;
    settings.fullRangeExport = false; settings.chartTitle = "WRONG LIVE TITLE"; settings.intro.mode = "auto";
    settings.audioSettings.trackVolume = 0.37; settings.tapFlowSpeed = 6; settings.clockCountEnabled = false;
    settings.intro.enabled = true; settings.intro.cardShadow = false; settings.intro.fontBodyPath = "/chosen-font.ttf";
    settings.preset = VideoExportPreset::HighQuality;
    settings.sizePreset = VideoExportSizePreset::UltraCompact;
    settings.audioBitrateKbps = 320;
    std::atomic_bool canceled{false};
    auto plan = miacode::android::prepareMobileBatchExport({a, a, bad, missing, b}, {5, 5, 6}, output, settings, canceled);
    check(!plan.canceled && plan.jobs.size() == 4 && plan.failures.size() == 3, "valid items survive invalid offset and absent media, duplicates are omitted");
    check(plan.jobs[0].task.outputPath != plan.jobs[2].task.outputPath, "equal chart titles do not overwrite one another");
    const auto& task = plan.jobs.first().task;
    check(task.chartTitle == "Same / title" && task.chartDesigner == "Chart author" && task.intro.artist == "Queue artist", "metadata belongs to each input chart");
    check(task.fullRangeExport && task.exportStartSeconds == 0 && task.contentDurationSeconds > 3, "batch ignores the live selection and applies the v2 chart tail");
    check(qAbs(task.noteMarkers.first().second - 0.25) < 0.001 && task.clockCount == 2 && qAbs(task.clockBpm - 174.5) < 0.001,
        "per-chart offset and wholebpm drive the captured clock and notes");
    check(qAbs(task.audioSettings.trackVolume - 0.37) < 0.001 && task.tapFlowSpeed == 6 && !task.clockCountEnabled,
        "audio and preview settings carry through the batch snapshot");
    check(task.preset == VideoExportPreset::HighQuality && task.sizePreset == VideoExportSizePreset::UltraCompact
        && task.audioBitrateKbps == 320, "batch preserves quality, size and requested audio rate for encoder policy");
    check(task.intro.enabled && !task.intro.cardShadow && task.intro.fontBodyPath == "/chosen-font.ttf"
        && task.intro.designer == "Chart author", "intro uses each chart's metadata and shared styling");
    check(plan.jobs[1].task.intro.difficulty == "ReMASTER", "ReMaster selects the correct intro atlas");
    write(task.outputPath, "existing");
    auto again = miacode::android::prepareMobileBatchExport({a}, {5}, output, settings, canceled);
    check(again.jobs.first().task.outputPath != task.outputPath, "existing private outputs are retained");
    auto unmatched = miacode::android::prepareMobileBatchExport({a}, {2}, output, settings, canceled);
    check(unmatched.jobs.isEmpty() && unmatched.failures.size() == 1, "absent selected difficulty is reported");
    const auto invalidEncoding = chart(directory.path(), "invalid-encoding", "0");
    write(invalidEncoding + "/maidata.txt", QByteArray("&title=bad\xff\n&first=0\n&inote_5=(120){4}1,E\n"));
    auto malformed = miacode::android::prepareMobileBatchExport({invalidEncoding}, {5}, output, settings, canceled);
    check(malformed.jobs.isEmpty() && malformed.failures.size() == 1, "invalid UTF-8 is rejected before chart parsing");
    canceled.store(true);
    auto stopped = miacode::android::prepareMobileBatchExport({a, b}, {5}, output, settings, canceled);
    check(stopped.canceled && stopped.jobs.isEmpty(), "preparation respects cancellation");
    const auto longName = miacode::android::mobileExportFileStem(QString(200, QChar(0x4e2d)), "fallback");
    check(longName.toUtf8().size() <= 180 && !longName.isEmpty(), "Unicode filenames fit the device filesystem");
    std::fprintf(stderr, "Mobile batch snapshot spec: %s\n", failures ? "failed" : "passed");
    return failures ? 1 : 0;
}
