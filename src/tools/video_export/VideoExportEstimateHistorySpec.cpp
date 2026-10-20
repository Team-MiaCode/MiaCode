#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTextStream>

#include "export/video_export/VideoExportEstimateHistory.h"

namespace {

bool require(bool condition, const QString& message, QTextStream& err)
{
    if (!condition) {
        err << message << Qt::endl;
        return false;
    }
    return true;
}

bool verifyHistory(QTextStream& err)
{
    using namespace miacode::video_export;

    QTemporaryDir directory;
    if (!require(directory.isValid(), QStringLiteral("temporary directory unavailable"), err)) {
        return false;
    }
    const QString path = directory.filePath(QStringLiteral("history.json"));
    ExportEstimateProfile standard{
        1920, 1080, 60, 120.0,
        VideoExportPreset::Fast,
        VideoExportSizePreset::Standard};
    if (!require(predictedExportTotalMs(standard, path) < 0,
                 QStringLiteral("empty history must not invent an estimate"), err)
        || !require(recordSuccessfulExportPerformance(standard, 60000, path),
                    QStringLiteral("valid performance sample was not saved"), err)) {
        return false;
    }

    ExportEstimateProfile audioOnly = standard;
    audioOnly.outputMode = VideoExportOutputMode::Wav;
    if (!require(predictedExportTotalMs(audioOnly, path) < 0,
                 QStringLiteral("WAV performance is isolated from video performance"), err)) {
        return false;
    }
    const qint64 exactEstimate = predictedExportTotalMs(standard, path);
    ExportEstimateProfile twiceAsLong = standard;
    twiceAsLong.durationSeconds = 240.0;
    const qint64 scaledEstimate = predictedExportTotalMs(twiceAsLong, path);
    ExportEstimateProfile otherPreset = standard;
    otherPreset.preset = VideoExportPreset::HighQuality;
    ExportEstimateProfile otherSizePreset = standard;
    otherSizePreset.sizePreset = VideoExportSizePreset::Compact;
    if (!require(exactEstimate == 60000,
                 QStringLiteral("exact profile estimate must reproduce its sample"), err)
        || !require(scaledEstimate == 120000,
                    QStringLiteral("estimate must scale with rendered work"), err)
        || !require(predictedExportTotalMs(otherPreset, path) < 0,
                    QStringLiteral("quality presets must not share performance samples"), err)
        || !require(predictedExportTotalMs(otherSizePreset, path) < 0,
                    QStringLiteral("size presets must not share performance samples"), err)) {
        return false;
    }

    QFile file(path);
    if (!require(file.open(QIODevice::ReadOnly), QStringLiteral("saved history cannot be opened"), err)) {
        return false;
    }
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    const QByteArray serialized = QJsonDocument(root).toJson(QJsonDocument::Compact);
    return require(root.value(QStringLiteral("schema")).toString()
                       == QStringLiteral("miacode_export_performance_v1"),
                   QStringLiteral("history schema mismatch"), err)
        && require(!serialized.contains("path")
                       && !serialized.contains("title")
                       && !serialized.contains("artist")
                       && !serialized.contains("designer"),
                   QStringLiteral("history must not persist identifying chart metadata"), err);
}

}  // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream err(stderr);
    QTextStream out(stdout);
    if (!verifyHistory(err)) {
        return 1;
    }
    out << "video_export_estimate_history_spec ok" << Qt::endl;
    return 0;
}
