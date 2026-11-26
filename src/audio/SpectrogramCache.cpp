#include "audio/SpectrogramCache.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QPointer>
#include <QSaveFile>
#include <QThreadPool>
#include <QtMath>

#include "audio/AudioFileDecoder.h"
#include "audio/WaveformCache.h"
#include "common/DebugLog.h"
#include "common/DebugOptions.h"

namespace miacode::spectrogram {

int spectrogramFftSize(int sampleRate)
{
    Q_UNUSED(sampleRate);
    return kSpectrogramFftSize;
}

double spectrogramMaxHz(int sampleRate)
{
    Q_UNUSED(sampleRate);
    return normalizedSpectrogramParameters({}).maxHz;
}

SpectrogramParameters normalizedSpectrogramParameters(SpectrogramParameters parameters)
{
    parameters.minHz = qBound(20.0, static_cast<double>(qRound(parameters.minHz)), 8000.0);
    const double lowestMaxHz = std::min(16000.0, std::max(parameters.minHz * 2.0, parameters.minHz + 1.0));
    parameters.maxHz = qBound(lowestMaxHz, static_cast<double>(qRound(parameters.maxHz)), 16000.0);
    const auto clampDb = [](float db) {
        return qBound(-80.0f, static_cast<float>(qRound(db)), -12.0f);
    };
    parameters.floorDb = qBound(-80.0f, static_cast<float>(qRound(parameters.floorDb)), 6.0f);
    parameters.ceilingDb = qBound(
        parameters.floorDb + 6.0f,
        static_cast<float>(qRound(parameters.ceilingDb)),
        12.0f);
    parameters.columnGateDb = clampDb(parameters.columnGateDb);
    parameters.prominenceFloorDb = clampDb(parameters.prominenceFloorDb);
    return parameters;
}

QString spectrogramParameterKey(const SpectrogramParameters& parameters)
{
    const SpectrogramParameters normalized = normalizedSpectrogramParameters(parameters);
    return QStringLiteral("%1|%2|%3|%4|%5|%6")
        .arg(normalized.minHz, 0, 'f', 0)
        .arg(normalized.maxHz, 0, 'f', 0)
        .arg(normalized.floorDb, 0, 'f', 0)
        .arg(normalized.ceilingDb, 0, 'f', 0)
        .arg(normalized.columnGateDb, 0, 'f', 0)
        .arg(normalized.prominenceFloorDb, 0, 'f', 0);
}

namespace {

constexpr quint32 kSpectrogramCacheMagic = 0x4D435346;  // MCSF
constexpr quint32 kSpectrogramCacheCurrentVersion = kSpectrogramCacheSchemaVersion;
constexpr quint32 kMaximumSpectrogramCacheLevelCount = 64;
constexpr float kSpectrogramDbFloor = -60.0f;
constexpr float kPi = 3.14159265358979323846f;

void appendSpectrogramDebugLog(const QString& payload)
{
    if (!miacode::debug_options::audioDebugOutputEnabled()) {
        return;
    }
    miacode::debug_log::appendLine(
        miacode::debug_log::Channel::Audio,
        QStringLiteral("preview/spectrogram"),
        payload);
}

QString spectrogramDebugSummary(const SpectrogramData& data)
{
    const SpectrogramLevel* top = data.levels.isEmpty() ? nullptr : &data.levels.constFirst();
    return QStringLiteral("duration=%1 levels=%2 columns=%3 bands=%4 ms_per_column=%5 empty=%6")
        .arg(data.durationSeconds, 0, 'f', 3)
        .arg(data.levels.size())
        .arg(top != nullptr ? top->columnCount() : 0)
        .arg(top != nullptr ? top->bandCount : 0)
        .arg(top != nullptr ? top->secondsPerColumn * 1000.0 : 0.0, 0, 'f', 3)
        .arg(data.isEmpty() ? 1 : 0);
}

double bandEdgeHz(int edge, double minHz, double maxHz)
{
    const double t = static_cast<double>(edge) / static_cast<double>(kSpectrogramBandCount);
    return minHz * std::pow(maxHz / minHz, t);
}

void fftRadix2(std::vector<std::complex<float>>& values)
{
    const int count = static_cast<int>(values.size());
    for (int index = 1, reversed = 0; index < count; ++index) {
        int bit = count >> 1;
        for (; reversed & bit; bit >>= 1) {
            reversed ^= bit;
        }
        reversed ^= bit;
        if (index < reversed) {
            std::swap(values[static_cast<size_t>(index)], values[static_cast<size_t>(reversed)]);
        }
    }
    for (int length = 2; length <= count; length <<= 1) {
        const float angle = -2.0f * kPi / static_cast<float>(length);
        const std::complex<float> step(std::cos(angle), std::sin(angle));
        for (int start = 0; start < count; start += length) {
            std::complex<float> twiddle(1.0f, 0.0f);
            const int half = length / 2;
            for (int offset = 0; offset < half; ++offset) {
                const size_t leftIndex = static_cast<size_t>(start + offset);
                const size_t rightIndex = static_cast<size_t>(start + offset + half);
                const std::complex<float> left = values[leftIndex];
                const std::complex<float> right = values[rightIndex] * twiddle;
                values[leftIndex] = left + right;
                values[rightIndex] = left - right;
                twiddle *= step;
            }
        }
    }
}

QVector<int> bandBinStarts(int sampleRate, int fftSize, double minHz, double maxHz)
{
    QVector<int> starts(kSpectrogramBandCount + 1, 1);
    const int nyquistBin = fftSize / 2;
    for (int edge = 0; edge <= kSpectrogramBandCount; ++edge) {
        const double hz = bandEdgeHz(edge, minHz, maxHz);
        const int bin = qRound(hz * static_cast<double>(fftSize) / static_cast<double>(sampleRate));
        starts[edge] = qBound(1, bin, nyquistBin);
    }
    for (int edge = 1; edge <= kSpectrogramBandCount; ++edge) {
        if (starts[edge] <= starts[edge - 1] && starts[edge - 1] < nyquistBin) {
            starts[edge] = starts[edge - 1] + 1;
        } else if (starts[edge] < starts[edge - 1]) {
            starts[edge] = starts[edge - 1];
        }
    }
    return starts;
}

float bandPowerDb(const std::vector<std::complex<float>>& spectrum, int beginBin, int endBin, int fftSize)
{
    if (endBin <= beginBin || fftSize <= 0) {
        return kSpectrogramDbFloor;
    }
    // Hann coherent gain is 0.5 and a pure sine lands in one bin at N/2,
    // so 4/N brings that sine to about 0 dB. Summing power, instead of
    // keeping only the peak bin, lets a drum hit add up across the band.
    const float scale = 4.0f / static_cast<float>(fftSize);
    float power = 0.0f;
    for (int bin = beginBin; bin < endBin; ++bin) {
        const float magnitude = std::abs(spectrum[static_cast<size_t>(bin)]) * scale;
        power += magnitude * magnitude;
    }
    return 10.0f * std::log10(std::max(power, 1.0e-10f));
}

float medianDb(const QVector<float>& values, int begin, int end)
{
    const int count = end - begin;
    if (count <= 0) {
        return kSpectrogramDbFloor;
    }
    std::vector<float> window;
    window.reserve(static_cast<size_t>(count));
    for (int index = begin; index < end; ++index) {
        window.push_back(values.at(index));
    }
    const auto middle = window.begin() + count / 2;
    std::nth_element(window.begin(), middle, window.end());
    return *middle;
}

// Loudness of one band. A drum hit fills many bands at once, so this is what
// keeps kicks, snares and cymbals visible.
float absoluteMagnitude(float db, float floorDb, float ceilingDb)
{
    const float span = std::max(1.0f, ceilingDb - floorDb);
    const float t = (db - floorDb) / span;
    if (t <= 0.0f) {
        return 0.0f;
    }
    return std::pow(std::min(t, 1.0f), kSpectrogramGamma);
}

// Extra brightness for a narrow partial that rises above the bands around it.
float prominenceMagnitude(float db, float envelopeDb, float floorDb)
{
    if (db < floorDb) {
        return 0.0f;
    }
    const float span = std::max(0.5f, kSpectrogramProminenceFullDb - kSpectrogramProminenceStartDb);
    const float t = (db - envelopeDb - kSpectrogramProminenceStartDb) / span;
    if (t <= 0.0f) {
        return 0.0f;
    }
    return std::pow(std::min(t, 1.0f), kSpectrogramProminenceGamma);
}

SpectrogramDataPtr buildSpectrogramData(
    const QString& normalizedTrackPath,
    qint64 fileSize,
    qint64 lastModifiedMs,
    const QVector<float>& samples,
    int sampleRate,
    double durationSeconds,
    const SpectrogramParameters& parameters)
{
    const SpectrogramParameters tuned = normalizedSpectrogramParameters(parameters);
    auto data = std::make_shared<SpectrogramData>();
    data->normalizedTrackPath = normalizedTrackPath;
    data->fileSize = fileSize;
    data->lastModifiedMs = lastModifiedMs;
    data->durationSeconds = qMax(0.0, durationSeconds);
    data->sampleRate = sampleRate;
    if (samples.isEmpty() || data->durationSeconds <= 0.0 || sampleRate <= 0) {
        return data;
    }

    const int columnCount = qMax(1, qRound(data->durationSeconds * kSpectrogramColumnsPerSecond));
    const int fftSize = spectrogramFftSize(sampleRate);
    const QVector<int> binStarts = bandBinStarts(sampleRate, fftSize, tuned.minHz, tuned.maxHz);
    std::vector<float> window(static_cast<size_t>(fftSize), 0.0f);
    for (int index = 0; index < fftSize; ++index) {
        window[static_cast<size_t>(index)] = 0.5f * (1.0f - std::cos(
            2.0f * kPi * static_cast<float>(index)
            / static_cast<float>(fftSize - 1)));
    }

    SpectrogramLevel topLevel;
    topLevel.secondsPerColumn = data->durationSeconds / static_cast<double>(columnCount);
    topLevel.bandCount = kSpectrogramBandCount;
    topLevel.magnitudes.resize(columnCount * kSpectrogramBandCount);
    const double samplesPerColumn = static_cast<double>(samples.size()) / static_cast<double>(columnCount);
    std::vector<std::complex<float>> spectrum(static_cast<size_t>(fftSize));
    QVector<float> bandDb(kSpectrogramBandCount);
    QVector<float> previous(kSpectrogramBandCount, 0.0f);
    for (int column = 0; column < columnCount; ++column) {
        const int center = qBound(
            0,
            qRound((static_cast<double>(column) + 0.5) * samplesPerColumn),
            samples.size() - 1);
        const int firstSample = center - fftSize / 2;
        for (int offset = 0; offset < fftSize; ++offset) {
            const int sampleIndex = firstSample + offset;
            const float sample = (sampleIndex >= 0 && sampleIndex < samples.size())
                ? samples.at(sampleIndex) : 0.0f;
            spectrum[static_cast<size_t>(offset)] = sample * window[static_cast<size_t>(offset)];
        }
        fftRadix2(spectrum);
        for (int band = 0; band < kSpectrogramBandCount; ++band) {
            bandDb[band] = bandPowerDb(spectrum, binStarts.at(band), binStarts.at(band + 1), fftSize);
        }
        float columnPeakDb = kSpectrogramDbFloor;
        for (float db : bandDb) {
            columnPeakDb = std::max(columnPeakDb, db);
        }
        // Same idea as a spectrogram gain control: lift a real hit so its
        // loudest band sits near 0 dB, without pushing silence up with it.
        if (columnPeakDb > tuned.columnGateDb) {
            const float lift = std::min(
                kSpectrogramColumnMaxLiftDb,
                std::max(0.0f, kSpectrogramColumnTargetDb - columnPeakDb));
            for (float& db : bandDb) {
                db += lift;
            }
        }
        float* destination = topLevel.magnitudes.data() + column * kSpectrogramBandCount;
        for (int band = 0; band < kSpectrogramBandCount; ++band) {
            const int envelopeBegin = qMax(0, band - kSpectrogramProminenceRadius);
            const int envelopeEnd = qMin(kSpectrogramBandCount, band + kSpectrogramProminenceRadius + 1);
            const float loudness = absoluteMagnitude(bandDb.at(band), tuned.floorDb, tuned.ceilingDb);
            const float ridge = prominenceMagnitude(
                bandDb.at(band),
                medianDb(bandDb, envelopeBegin, envelopeEnd),
                tuned.prominenceFloorDb);
            const float magnitude = std::max(loudness, ridge);
            const float onset = std::max(0.0f, magnitude - previous.at(band));
            destination[band] = std::min(1.0f, magnitude + kSpectrogramOnsetGain * onset);
            previous[band] = magnitude;
        }
    }

    data->levels.append(topLevel);
    while (data->levels.constLast().columnCount() > 1) {
        const SpectrogramLevel& previous = data->levels.constLast();
        SpectrogramLevel next;
        next.secondsPerColumn = previous.secondsPerColumn * 2.0;
        next.bandCount = previous.bandCount;
        const int nextColumns = (previous.columnCount() + 1) / 2;
        next.magnitudes.resize(nextColumns * next.bandCount);
        for (int column = 0; column < nextColumns; ++column) {
            const int first = column * 2;
            const int second = qMin(first + 1, previous.columnCount() - 1);
            float* destination = next.magnitudes.data() + column * next.bandCount;
            for (int band = 0; band < next.bandCount; ++band) {
                destination[band] = std::max(
                    previous.magnitudeAt(first, band),
                    previous.magnitudeAt(second, band));
            }
        }
        data->levels.append(next);
    }
    return data;
}

}  // namespace

int SpectrogramLevel::columnCount() const
{
    return bandCount > 0 ? magnitudes.size() / bandCount : 0;
}

double SpectrogramLevel::columnsPerSecond() const
{
    return secondsPerColumn > 0.0 ? (1.0 / secondsPerColumn) : 0.0;
}

float SpectrogramLevel::magnitudeAt(int column, int band) const
{
    if (band < 0 || band >= bandCount || column < 0 || column >= columnCount()) {
        return 0.0f;
    }
    return magnitudes.at(column * bandCount + band);
}

bool SpectrogramData::isEmpty() const
{
    return levels.isEmpty() || levels.constFirst().columnCount() <= 0;
}

QString spectrogramCacheDirectoryPath(const QString& projectDataDirectoryPath)
{
    if (projectDataDirectoryPath.isEmpty()) {
        return QString();
    }
    return QDir(projectDataDirectoryPath).filePath(QStringLiteral("spectrogram"));
}

QString spectrogramCacheFilePath(
    const QString& normalizedTrackPath,
    const QString& cacheDirectoryPath,
    const SpectrogramParameters& parameters)
{
    if (normalizedTrackPath.isEmpty() || cacheDirectoryPath.isEmpty()) {
        return QString();
    }
    const QByteArray hash = QCryptographicHash::hash(
        (normalizedTrackPath + QLatin1Char('|') + spectrogramParameterKey(parameters)).toUtf8(),
        QCryptographicHash::Sha256).toHex();
    return QDir(cacheDirectoryPath).filePath(QStringLiteral("%1.spec").arg(QString::fromLatin1(hash)));
}

int bandContainingHz(double hz, int sampleRate)
{
    Q_UNUSED(sampleRate);
    const SpectrogramParameters tuned = normalizedSpectrogramParameters({});
    if (!(hz > 0.0) || hz < tuned.minHz) {
        return 0;
    }
    if (hz >= tuned.maxHz) {
        return kSpectrogramBandCount - 1;
    }
    const double t = std::log(hz / tuned.minHz) / std::log(tuned.maxHz / tuned.minHz);
    return qBound(0, static_cast<int>(std::floor(t * kSpectrogramBandCount)), kSpectrogramBandCount - 1);
}

SpectrogramDataPtr makeSpectrogramPlaceholder(double durationSeconds)
{
    auto data = std::make_shared<SpectrogramData>();
    data->durationSeconds = qMax(0.0, durationSeconds);
    return data;
}

SpectrogramDataPtr buildSpectrogramDataFromSamples(
    const QString& trackPath,
    qint64 fileSize,
    qint64 lastModifiedMs,
    const QVector<float>& samples,
    int sampleRate,
    double durationSeconds,
    const SpectrogramParameters& parameters)
{
    return buildSpectrogramData(
        miacode::waveform::normalizeTrackPath(trackPath),
        fileSize,
        lastModifiedMs,
        samples,
        sampleRate,
        durationSeconds,
        parameters);
}

SpectrogramDataPtr buildSpectrogramDataFromFile(
    const QString& trackPath,
    qint64 fileSize,
    qint64 lastModifiedMs,
    const miacode::audio_decode::AudioFileDecoder* decoder,
    const SpectrogramParameters& parameters)
{
    QElapsedTimer timer;
    timer.start();
    miacode::audio_decode::DecodedMonoAudio decoded;
    if (decoder != nullptr) {
        decoded = decoder->decodeFileToMono(trackPath, 0);
    }
    SpectrogramDataPtr data = buildSpectrogramData(
        miacode::waveform::normalizeTrackPath(trackPath),
        fileSize,
        lastModifiedMs,
        decoded.samples,
        decoded.sampleRate > 0 ? decoded.sampleRate : miacode::waveform::kWaveformDecodeSampleRate,
        decoded.durationSeconds,
        parameters);
    appendSpectrogramDebugLog(
        QStringLiteral("event=build decoder=%1 rate=%2 samples=%3 elapsed_ms=%4 %5")
            .arg(decoder != nullptr ? decoder->decoderName() : QStringLiteral("none"))
            .arg(decoded.sampleRate)
            .arg(decoded.samples.size())
            .arg(timer.nsecsElapsed() / 1000000.0, 0, 'f', 3)
            .arg(data ? spectrogramDebugSummary(*data) : QStringLiteral("data=0")));
    return data;
}

bool spectrogramParametersMatch(const SpectrogramParameters& stored, const SpectrogramParameters& expected)
{
    const SpectrogramParameters left = normalizedSpectrogramParameters(stored);
    const SpectrogramParameters right = normalizedSpectrogramParameters(expected);
    return qAbs(left.minHz - right.minHz) < 0.5
        && qAbs(left.maxHz - right.maxHz) < 0.5
        && qAbs(left.floorDb - right.floorDb) < 0.5f
        && qAbs(left.ceilingDb - right.ceilingDb) < 0.5f
        && qAbs(left.columnGateDb - right.columnGateDb) < 0.5f
        && qAbs(left.prominenceFloorDb - right.prominenceFloorDb) < 0.5f;
}

SpectrogramDataPtr readSpectrogramDataCache(
    const QString& cacheFilePath,
    const QString& normalizedTrackPath,
    qint64 expectedFileSize,
    qint64 expectedLastModifiedMs,
    const SpectrogramParameters& parameters)
{
    QFile file(cacheFilePath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_6_0);
    stream.setByteOrder(QDataStream::LittleEndian);
    quint32 magic = 0;
    quint32 version = 0;
    qint64 storedFileSize = -1;
    qint64 storedLastModifiedMs = -1;
    double durationSeconds = 0.0;
    qint32 bandCount = 0;
    quint32 levelCount = 0;
    double storedMinHz = 0.0;
    double storedMaxHz = 0.0;
    float storedFloorDb = 0.0f;
    float storedCeilingDb = 0.0f;
    float storedColumnGateDb = 0.0f;
    float storedProminenceFloorDb = 0.0f;
    qint32 storedSampleRate = 0;
    stream >> magic >> version >> storedFileSize >> storedLastModifiedMs
        >> durationSeconds >> bandCount >> levelCount
        >> storedMinHz >> storedMaxHz >> storedFloorDb >> storedCeilingDb
        >> storedColumnGateDb >> storedProminenceFloorDb >> storedSampleRate;
    if (stream.status() != QDataStream::Ok
        || magic != kSpectrogramCacheMagic
        || version != kSpectrogramCacheCurrentVersion
        || storedFileSize != expectedFileSize
        || storedLastModifiedMs != expectedLastModifiedMs
        || !spectrogramParametersMatch(
            SpectrogramParameters{
                storedMinHz, storedMaxHz, storedFloorDb, storedCeilingDb, storedColumnGateDb, storedProminenceFloorDb},
            parameters)
        || !qIsFinite(durationSeconds)
        || bandCount != kSpectrogramBandCount
        || levelCount == 0
        || levelCount > kMaximumSpectrogramCacheLevelCount) {
        return {};
    }

    auto data = std::make_shared<SpectrogramData>();
    data->normalizedTrackPath = normalizedTrackPath;
    data->fileSize = storedFileSize;
    data->lastModifiedMs = storedLastModifiedMs;
    data->durationSeconds = qMax(0.0, durationSeconds);
    data->sampleRate = storedSampleRate;
    data->levels.reserve(static_cast<int>(levelCount));
    for (quint32 levelIndex = 0; levelIndex < levelCount; ++levelIndex) {
        qint32 columnCount = 0;
        double secondsPerColumn = 0.0;
        stream >> columnCount >> secondsPerColumn;
        const qint64 valueCount = static_cast<qint64>(columnCount) * bandCount;
        const qint64 maximumValueCount = file.bytesAvailable() / static_cast<qint64>(sizeof(float));
        if (stream.status() != QDataStream::Ok
            || columnCount <= 0
            || valueCount <= 0
            || valueCount > maximumValueCount
            || !qIsFinite(secondsPerColumn)
            || secondsPerColumn <= 0.0) {
            return {};
        }
        SpectrogramLevel level;
        level.secondsPerColumn = secondsPerColumn;
        level.bandCount = bandCount;
        level.magnitudes.resize(static_cast<int>(valueCount));
        for (int valueIndex = 0; valueIndex < level.magnitudes.size(); ++valueIndex) {
            float magnitude = 0.0f;
            stream >> magnitude;
            if (stream.status() != QDataStream::Ok) {
                return {};
            }
            level.magnitudes[valueIndex] = qBound(0.0f, magnitude, 1.0f);
        }
        data->levels.append(level);
    }
    return data->isEmpty() ? SpectrogramDataPtr{} : data;
}

bool writeSpectrogramDataCache(
    const QString& cacheFilePath,
    const SpectrogramData& data,
    const SpectrogramParameters& parameters)
{
    if (cacheFilePath.isEmpty() || data.levels.isEmpty()) {
        return false;
    }
    const QFileInfo cacheInfo(cacheFilePath);
    if (!QDir().mkpath(cacheInfo.absolutePath())) {
        return false;
    }
    QSaveFile file(cacheFilePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_6_0);
    stream.setByteOrder(QDataStream::LittleEndian);
    const SpectrogramParameters tuned = normalizedSpectrogramParameters(parameters);
    stream << kSpectrogramCacheMagic
           << kSpectrogramCacheCurrentVersion
           << data.fileSize
           << data.lastModifiedMs
           << data.durationSeconds
           << static_cast<qint32>(kSpectrogramBandCount)
           << static_cast<quint32>(data.levels.size())
           << tuned.minHz
           << tuned.maxHz
           << tuned.floorDb
           << tuned.ceilingDb
           << tuned.columnGateDb
           << tuned.prominenceFloorDb
           << static_cast<qint32>(data.sampleRate);
    for (const SpectrogramLevel& level : data.levels) {
        stream << static_cast<qint32>(level.columnCount()) << level.secondsPerColumn;
        for (float magnitude : level.magnitudes) {
            stream << magnitude;
        }
    }
    if (stream.status() != QDataStream::Ok) {
        return false;
    }
    return file.commit();
}

const SpectrogramLevel* selectSpectrogramLevelForVisibleRange(
    const SpectrogramData& data,
    double visibleDurationSeconds,
    int targetPixelWidth)
{
    if (data.levels.isEmpty() || visibleDurationSeconds <= 0.0 || targetPixelWidth <= 0) {
        return nullptr;
    }
    const double targetColumnsPerSecond = static_cast<double>(targetPixelWidth) / visibleDurationSeconds;
    const SpectrogramLevel* selected = &data.levels.constFirst();
    for (const SpectrogramLevel& level : data.levels) {
        if (level.columnsPerSecond() + 1e-6 >= targetColumnsPerSecond) {
            selected = &level;
            continue;
        }
        break;
    }
    return selected;
}

QPair<int, int> visibleSpectrogramColumnRange(
    const SpectrogramLevel& level,
    double visibleStartSecond,
    double visibleEndSecond)
{
    if (level.columnCount() <= 0 || level.secondsPerColumn <= 0.0) {
        return {0, 0};
    }
    const double boundedStart = qMin(visibleStartSecond, visibleEndSecond);
    const double boundedEnd = qMax(visibleStartSecond, visibleEndSecond);
    const int begin = qBound(
        0,
        static_cast<int>(std::floor(qMax(0.0, boundedStart) / level.secondsPerColumn)),
        level.columnCount());
    const int end = qBound(
        begin,
        static_cast<int>(std::ceil(qMax(0.0, boundedEnd) / level.secondsPerColumn)) + 1,
        level.columnCount());
    return {begin, end};
}

SpectrogramCacheService::SpectrogramCacheService(QObject* parent)
    : QObject(parent)
{}

void SpectrogramCacheService::setThreadPool(QThreadPool* threadPool)
{
    threadPool_ = threadPool;
}

void SpectrogramCacheService::setDecoder(
    std::shared_ptr<const miacode::audio_decode::AudioFileDecoder> decoder)
{
    decoder_ = std::move(decoder);
}

void SpectrogramCacheService::requestSpectrogram(
    const QString& trackPath,
    const QString& cacheDirectoryPath,
    RequestCallback callback,
    const SpectrogramParameters& parameters)
{
    const QString normalizedTrackPath = miacode::waveform::normalizeTrackPath(trackPath);
    const QFileInfo trackInfo(trackPath);
    if (normalizedTrackPath.isEmpty() || !trackInfo.exists() || !trackInfo.isFile()) {
        if (callback) {
            callback(makeSpectrogramPlaceholder(0.0));
        }
        return;
    }

    const qint64 fileSize = trackInfo.size();
    const qint64 lastModifiedMs = trackInfo.lastModified().toMSecsSinceEpoch();
    const SpectrogramParameters tuned = normalizedSpectrogramParameters(parameters);
    const QString cacheKey = cacheKeyForTrack(normalizedTrackPath, fileSize, lastModifiedMs, tuned);
    if (memoryCache_.contains(cacheKey)) {
        if (callback) {
            callback(memoryCache_.value(cacheKey));
        }
        return;
    }
    if (pendingRequests_.contains(cacheKey)) {
        if (callback) {
            pendingRequests_.value(cacheKey)->callbacks.append(std::move(callback));
        }
        return;
    }

    auto request = std::make_shared<PendingRequest>();
    request->cacheKey = cacheKey;
    request->trackPath = trackPath;
    request->normalizedTrackPath = normalizedTrackPath;
    request->cacheFilePath = spectrogramCacheFilePath(normalizedTrackPath, cacheDirectoryPath, tuned);
    request->fileSize = fileSize;
    request->lastModifiedMs = lastModifiedMs;
    request->parameters = tuned;
    if (callback) {
        request->callbacks.append(std::move(callback));
    }
    pendingRequests_.insert(cacheKey, request);
    appendSpectrogramDebugLog(
        QStringLiteral("event=request source=worker track_id=%1 file_size=%2 tune=%3")
            .arg(miacode::waveform::waveformTrackDebugId(normalizedTrackPath))
            .arg(fileSize)
            .arg(spectrogramParameterKey(tuned)));

    QPointer<SpectrogramCacheService> guard(this);
    QThreadPool* const pool = threadPool_ != nullptr ? threadPool_ : QThreadPool::globalInstance();
    pool->start([guard, request, decoder = decoder_]() {
        SpectrogramDataPtr data;
        if (!request->cacheFilePath.isEmpty()) {
            data = readSpectrogramDataCache(
                request->cacheFilePath,
                request->normalizedTrackPath,
                request->fileSize,
                request->lastModifiedMs,
                request->parameters);
        }
        if (!data) {
            data = buildSpectrogramDataFromFile(
                request->trackPath,
                request->fileSize,
                request->lastModifiedMs,
                decoder.get(),
                request->parameters);
            if (data && !data->levels.isEmpty() && !request->cacheFilePath.isEmpty()) {
                writeSpectrogramDataCache(request->cacheFilePath, *data, request->parameters);
            }
        }
        if (!data) {
            data = makeSpectrogramPlaceholder(0.0);
        }
        if (guard.isNull()) {
            return;
        }
        QMetaObject::invokeMethod(
            guard.data(),
            [guard, request, data]() {
                if (!guard.isNull()) {
                    guard->finishPendingRequest(request, data);
                }
            },
            Qt::QueuedConnection);
    });
}

void SpectrogramCacheService::clear()
{
    memoryCache_.clear();
    pendingRequests_.clear();
}

QString SpectrogramCacheService::cacheKeyForTrack(
    const QString& trackPath,
    qint64 fileSize,
    qint64 lastModifiedMs,
    const SpectrogramParameters& parameters) const
{
    return QStringLiteral("%1|%2|%3|%4|%5")
        .arg(trackPath)
        .arg(fileSize)
        .arg(lastModifiedMs)
        .arg(kSpectrogramCacheCurrentVersion)
        .arg(spectrogramParameterKey(parameters));
}

void SpectrogramCacheService::finishPendingRequest(
    const std::shared_ptr<PendingRequest>& request,
    SpectrogramDataPtr data)
{
    const auto it = pendingRequests_.find(request->cacheKey);
    if (it == pendingRequests_.end() || it.value() != request) {
        return;
    }
    pendingRequests_.erase(it);
    if (data) {
        memoryCache_.insert(request->cacheKey, data);
    }
    for (const RequestCallback& callback : request->callbacks) {
        if (callback) {
            callback(data);
        }
    }
}

}  // namespace miacode::spectrogram
