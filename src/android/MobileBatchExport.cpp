#include "MobileBatchExport.h"
#include "MobileExportTask.h"
#include "common/ChartAssetPaths.h"
#include <QAudioDecoder>
#include <QAudioBuffer>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QSet>
#include <QStringDecoder>
#include <QTimer>
#include <QUrl>
#ifdef Q_OS_ANDROID
#include <QJniEnvironment>
#include <QJniObject>
#endif

namespace miacode::android {
namespace {
double trackDuration(const QString& path, const std::atomic_bool& canceled, QString* error) {
#ifdef Q_OS_ANDROID
    if (canceled.load()) return 0;
    QJniObject retriever("android/media/MediaMetadataRetriever");
    QJniEnvironment environment;
    retriever.callMethod<void>("setDataSource", "(Ljava/lang/String;)V", QJniObject::fromString(path).object<jstring>());
    const bool failed = environment.checkAndClearExceptions();
    bool ok = false;
    double duration = 0;
    if (!failed) duration = retriever.callObjectMethod("extractMetadata", "(I)Ljava/lang/String;", 9).toString().toDouble(&ok) / 1000;
    const bool metadataFailed = environment.checkAndClearExceptions();
    retriever.callMethod<void>("release");
    environment.checkAndClearExceptions();
    if (failed || metadataFailed || !ok || !qIsFinite(duration) || duration <= 0) {
        *error = QStringLiteral("Cannot read the chart's audio duration"); return 0;
    }
    return duration;
#else
    // Count frames without retaining the decoded song in memory.
    QAudioDecoder decoder;
    decoder.setSource(QUrl::fromLocalFile(path));
    QEventLoop loop;
    QTimer timeout, cancellation;
    timeout.setSingleShot(true); cancellation.setInterval(50);
    bool finished = false;
    double duration = 0;
    QObject::connect(&decoder, &QAudioDecoder::bufferReady, &loop, [&] {
        const auto buffer = decoder.read();
        if (buffer.isValid()) duration += static_cast<double>(buffer.frameCount()) / buffer.format().sampleRate();
    });
    QObject::connect(&decoder, &QAudioDecoder::finished, &loop, [&] { finished = true; loop.quit(); });
    QObject::connect(&decoder, qOverload<QAudioDecoder::Error>(&QAudioDecoder::error), &loop,
        [&](QAudioDecoder::Error) { *error = decoder.errorString(); loop.quit(); });
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&] { *error = QStringLiteral("Audio duration probe timed out"); loop.quit(); });
    QObject::connect(&cancellation, &QTimer::timeout, &loop, [&] { if (canceled.load()) loop.quit(); });
    timeout.start(120000); cancellation.start(); decoder.start(); loop.exec(); decoder.stop();
    if (!finished && error->isEmpty() && !canceled.load()) *error = QStringLiteral("Audio duration probe failed");
    return finished ? duration : 0;
#endif
}
QString resolveChart(const QDir& directory) {
    const auto preferred = directory.filePath("maidata.txt");
    if (QFileInfo::exists(preferred)) return preferred;
    const auto candidates = directory.entryList({"*.txt", "*.simai"}, QDir::Files, QDir::Name);
    return candidates.isEmpty() ? QString() : directory.filePath(candidates.first());
}
}
MobileBatchExportPlan prepareMobileBatchExport(const QStringList& directories, const QList<int>& difficulties,
    const QString& outputDirectory, const VideoExportTask& settings, const std::atomic_bool& canceled) {
    MobileBatchExportPlan plan;
    QSet<QString> seenDirectories, outputPaths;
    QStringList requestedLabels;
    for (const int id : difficulties) requestedLabels.append(SimaiDocument::difficultyShortName(id));
    for (const auto& input : directories) {
        if (canceled.load()) { plan.canceled = true; break; }
        const QDir directory(input);
        const auto canonical = directory.canonicalPath();
        if (!canonical.isEmpty() && seenDirectories.contains(canonical)) continue;
        seenDirectories.insert(canonical);
        const auto fail = [&](const QString& message) { plan.failures.append(input + " - " + message); };
        if (!directory.exists()) { fail(qtTrId("dialog.batch_export.error.invalid_folder")); continue; }
        const auto chartPath = resolveChart(directory);
        const auto audioPath = chart_assets::resolveTrackPathForDirectory(directory.absolutePath());
        if (chartPath.isEmpty() || audioPath.isEmpty()) {
            fail(qtTrId(chartPath.isEmpty() ? "dialog.batch_export.error.missing_chart_file" : "dialog.batch_export.error.missing_track_file")); continue;
        }
        QFile chart(chartPath);
        if (!chart.open(QIODevice::ReadOnly) || chart.size() > 16 * 1024 * 1024) {
            fail(qtTrId("dialog.batch_export.error.read_chart_failed").arg(QFileInfo(chartPath).fileName())); continue;
        }
        const auto bytes = chart.readAll();
        const auto encoding = QStringConverter::encodingForData(bytes).value_or(QStringConverter::Utf8);
        QStringDecoder decoder(encoding);
        const QString source = decoder(bytes);
        if (decoder.hasError()) { fail(QStringLiteral("Chart text must use UTF-8 or a supported Unicode encoding")); continue; }
        const auto document = SimaiDocument::fromText(source);
        QString probeError;
        const double duration = trackDuration(audioPath, canceled, &probeError);
        if (canceled.load()) { plan.canceled = true; break; }
        if (!probeError.isEmpty()) { fail(probeError); continue; }
        QSet<int> matched;
        for (const int id : difficulties) {
            if (canceled.load()) { plan.canceled = true; break; }
            if (!document.difficulty(id) || matched.contains(id)) continue;
            matched.insert(id);
            VideoExportTask task;
            QString failure;
            const auto token = SimaiDocument::difficultyShortName(id);
            const auto label = directory.dirName() + " [" + token + "]";
            if (!buildMobileChartExportTask(document, id, chartPath, duration, settings, &task, &failure)) {
                plan.failures.append(label + " - " + failure); continue;
            }
            const auto stem = mobileExportFileStem(document.title, directory.dirName()) + "_" + QString(token).replace(':', '_');
            const auto suffix = settings.outputPath.endsWith(".wav", Qt::CaseInsensitive) ? QStringLiteral(".wav") : QStringLiteral(".mp4");
            QString output = QDir(outputDirectory).filePath(stem + suffix);
            for (int duplicate = 1; outputPaths.contains(output) || QFileInfo::exists(output); ++duplicate)
                output = QDir(outputDirectory).filePath(stem + "(" + QString::number(duplicate) + ")" + suffix);
            outputPaths.insert(output); task.outputPath = output;
            plan.jobs.append({std::move(task), label});
        }
        if (matched.isEmpty() && !plan.canceled) fail(qtTrId("dialog.batch_export.error.no_selected_difficulties_in_folder")
            .arg(requestedLabels.join(", ")));
        if (plan.canceled) break;
    }
    return plan;
}
}
