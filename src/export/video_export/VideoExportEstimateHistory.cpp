#include "export/video_export/VideoExportEstimateHistory.h"
#include "export/video_export/VideoExportSnapshot.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>

namespace miacode::video_export {
namespace {

constexpr int kMaximumSamples = 64;
constexpr auto kHistorySchema = "miacode_export_performance_v1";

struct Sample {
    ExportEstimateProfile profile;
    qint64 elapsedMs = 0;
    QString createdAtUtc;
};

QString presetToken(VideoExportPreset preset)
{
    return preset == VideoExportPreset::Fast
        ? QStringLiteral("fast")
        : QStringLiteral("high_quality");
}

VideoExportSizePreset sizePresetFromToken(const QString& token)
{
    if (token == QLatin1String("compact")) {
        return VideoExportSizePreset::Compact;
    }
    if (token == QLatin1String("ultra_compact_with_pv")) {
        return VideoExportSizePreset::UltraCompactWithPv;
    }
    if (token == QLatin1String("ultra_compact")) {
        return VideoExportSizePreset::UltraCompact;
    }
    return VideoExportSizePreset::Standard;
}

QString sizePresetToken(VideoExportSizePreset preset)
{
    switch (preset) {
    case VideoExportSizePreset::Compact:
        return QStringLiteral("compact");
    case VideoExportSizePreset::UltraCompactWithPv:
        return QStringLiteral("ultra_compact_with_pv");
    case VideoExportSizePreset::UltraCompact:
        return QStringLiteral("ultra_compact");
    case VideoExportSizePreset::Standard:
    default:
        return QStringLiteral("standard");
    }
}

bool validProfile(const ExportEstimateProfile& profile)
{
    return profile.outputWidth >= 64 && profile.outputWidth <= 16384
        && profile.outputHeight >= 64 && profile.outputHeight <= 16384
        && profile.fps >= 1 && profile.fps <= 480
        && std::isfinite(profile.durationSeconds)
        && profile.durationSeconds >= 0.1 && profile.durationSeconds <= 24.0 * 60.0 * 60.0;
}

double workUnits(const ExportEstimateProfile& profile)
{
    return static_cast<double>(profile.outputWidth)
        * static_cast<double>(profile.outputHeight)
        * static_cast<double>(profile.fps)
        * profile.durationSeconds;
}

QString resolvedHistoryPath(const QString& overridePath)
{
    return overridePath.trimmed().isEmpty() ? exportEstimateHistoryPath() : overridePath;
}

QVector<Sample> loadSamples(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return {};
    }
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("schema")).toString() != QLatin1String(kHistorySchema)) {
        return {};
    }

    QVector<Sample> samples;
    const QJsonArray array = root.value(QStringLiteral("samples")).toArray();
    samples.reserve(qMin(array.size(), kMaximumSamples));
    for (const QJsonValue& value : array) {
        const QJsonObject object = value.toObject();
        Sample sample;
        sample.profile.outputWidth = object.value(QStringLiteral("width")).toInt();
        sample.profile.outputHeight = object.value(QStringLiteral("height")).toInt();
        sample.profile.fps = object.value(QStringLiteral("fps")).toInt();
        sample.profile.durationSeconds = object.value(QStringLiteral("duration_seconds")).toDouble();
        const QString preset = object.value(QStringLiteral("preset")).toString();
        sample.profile.preset = preset == QLatin1String("fast")
            ? VideoExportPreset::Fast
            : VideoExportPreset::HighQuality;
        sample.profile.sizePreset = sizePresetFromToken(
            object.value(QStringLiteral("size_preset")).toString());
        sample.profile.outputMode = videoExportOutputModeFromToken(object.value(QStringLiteral("output_mode")).toString());
        sample.elapsedMs = object.value(QStringLiteral("elapsed_ms")).toVariant().toLongLong();
        sample.createdAtUtc = object.value(QStringLiteral("created_at_utc")).toString();
        if (validProfile(sample.profile) && sample.elapsedMs >= 100 && sample.elapsedMs <= 7LL * 24LL * 60LL * 60LL * 1000LL) {
            samples.append(sample);
        }
    }
    if (samples.size() > kMaximumSamples) {
        samples = samples.mid(samples.size() - kMaximumSamples);
    }
    return samples;
}

QJsonObject sampleToJson(const Sample& sample)
{
    QJsonObject object;
    object.insert(QStringLiteral("created_at_utc"), sample.createdAtUtc);
    object.insert(QStringLiteral("width"), sample.profile.outputWidth);
    object.insert(QStringLiteral("height"), sample.profile.outputHeight);
    object.insert(QStringLiteral("fps"), sample.profile.fps);
    object.insert(QStringLiteral("duration_seconds"), sample.profile.durationSeconds);
    object.insert(QStringLiteral("preset"), presetToken(sample.profile.preset));
    object.insert(QStringLiteral("size_preset"), sizePresetToken(sample.profile.sizePreset));
    object.insert(QStringLiteral("output_mode"), videoExportOutputModeToken(sample.profile.outputMode));
    object.insert(QStringLiteral("elapsed_ms"), sample.elapsedMs);
    return object;
}

}  // namespace

ExportEstimateProfile exportEstimateProfileForSnapshot(const ::VideoExportSnapshot& snapshot)
{
    ExportEstimateProfile profile;
    profile.outputWidth = snapshot.outputWidth;
    profile.outputHeight = snapshot.outputHeight;
    profile.fps = snapshot.fps;
    profile.durationSeconds = snapshot.contentDurationSeconds;
    profile.preset = snapshot.preset;
    profile.sizePreset = snapshot.sizePreset;
    profile.outputMode = snapshot.outputMode;
    return profile;
}

QString exportEstimateHistoryPath()
{
    const QString root = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return QDir(root).filePath(QStringLiteral("export-performance-history.json"));
}

qint64 predictedExportTotalMs(
    const ExportEstimateProfile& profile,
    const QString& historyPathOverride)
{
    if (!validProfile(profile)) {
        return -1;
    }

    struct Candidate {
        double distance = 0.0;
        double predictedMs = 0.0;
    };
    QVector<Candidate> candidates;
    const double requestedWork = workUnits(profile);
    for (const Sample& sample : loadSamples(resolvedHistoryPath(historyPathOverride))) {
        if (sample.profile.outputMode != profile.outputMode
            || sample.profile.preset != profile.preset
            || sample.profile.sizePreset != profile.sizePreset) {
            continue;
        }
        const double sampleWork = workUnits(sample.profile);
        if (sampleWork <= 0.0) {
            continue;
        }
        const double pixelRatio = static_cast<double>(profile.outputWidth) * profile.outputHeight
            / (static_cast<double>(sample.profile.outputWidth) * sample.profile.outputHeight);
        const double fpsRatio = static_cast<double>(profile.fps) / sample.profile.fps;
        const double durationRatio = profile.durationSeconds / sample.profile.durationSeconds;
        Candidate candidate;
        candidate.distance = 1.5 * std::abs(std::log(pixelRatio))
            + 0.5 * std::abs(std::log(fpsRatio))
            + 0.25 * std::abs(std::log(durationRatio));
        candidate.predictedMs = static_cast<double>(sample.elapsedMs) * requestedWork / sampleWork;
        candidates.append(candidate);
    }
    if (candidates.isEmpty()) {
        return -1;
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& left, const Candidate& right) {
        return left.distance < right.distance;
    });
    if (candidates.size() > 8) {
        candidates.resize(8);
    }
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& left, const Candidate& right) {
        return left.predictedMs < right.predictedMs;
    });

    double totalWeight = 0.0;
    for (const Candidate& candidate : candidates) {
        totalWeight += 1.0 / (1.0 + candidate.distance);
    }
    double accumulatedWeight = 0.0;
    for (const Candidate& candidate : candidates) {
        accumulatedWeight += 1.0 / (1.0 + candidate.distance);
        if (accumulatedWeight * 2.0 >= totalWeight) {
            return qRound64(candidate.predictedMs);
        }
    }
    return qRound64(candidates.constLast().predictedMs);
}

bool recordSuccessfulExportPerformance(
    const ExportEstimateProfile& profile,
    qint64 elapsedMs,
    const QString& historyPathOverride)
{
    if (!validProfile(profile) || elapsedMs < 100 || elapsedMs > 7LL * 24LL * 60LL * 60LL * 1000LL) {
        return false;
    }
    const QString path = resolvedHistoryPath(historyPathOverride);
    QVector<Sample> samples = loadSamples(path);
    samples.append(Sample{profile, elapsedMs, QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)});
    if (samples.size() > kMaximumSamples) {
        samples.remove(0, samples.size() - kMaximumSamples);
    }

    QJsonArray array;
    for (const Sample& sample : samples) {
        array.append(sampleToJson(sample));
    }
    QJsonObject root;
    root.insert(QStringLiteral("schema"), QLatin1String(kHistorySchema));
    root.insert(QStringLiteral("samples"), array);

    const QFileInfo info(path);
    if (!QDir().mkpath(info.absolutePath())) {
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Indented);
    return file.write(payload) == payload.size() && file.commit();
}

}  // namespace miacode::video_export
