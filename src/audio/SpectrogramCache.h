#pragma once

#include <functional>
#include <memory>

#include <QHash>
#include <QObject>
#include <QPair>
#include <QString>
#include <QVector>

class QThreadPool;

namespace miacode::audio_decode {
class AudioFileDecoder;
}

namespace miacode::spectrogram {

// Time-frequency pyramid for the timeline spectrogram. Each band stores summed
// bin power. A real column is lifted so its peak sits near 0 dB, then the
// stored value is the louder of that level and a narrow ridge above its
// neighbors. Coarser levels max-pool adjacent columns.
inline constexpr quint32 kSpectrogramCacheSchemaVersion = 9;
inline constexpr int kSpectrogramBandCount = 256;
inline constexpr double kSpectrogramColumnsPerSecond = 192.0;
inline constexpr int kSpectrogramFftSize = 2048;
inline constexpr float kSpectrogramGamma = 0.75f;
inline constexpr float kSpectrogramColumnTargetDb = 0.0f;
inline constexpr float kSpectrogramColumnMaxLiftDb = 40.0f;
inline constexpr float kSpectrogramProminenceStartDb = 6.5f;
inline constexpr float kSpectrogramProminenceFullDb = 14.0f;
inline constexpr int kSpectrogramProminenceRadius = 9;
inline constexpr float kSpectrogramProminenceGamma = 1.35f;
inline constexpr float kSpectrogramOnsetGain = 1.5f;

// Per-song range and floors. The other analysis numbers stay fixed.
struct SpectrogramParameters {
    double minHz = 20.0;
    double maxHz = 1000.0;
    float floorDb = -30.0f;
    float ceilingDb = 0.0f;
    float columnGateDb = -30.0f;
    float prominenceFloorDb = -32.0f;
};

SpectrogramParameters normalizedSpectrogramParameters(SpectrogramParameters parameters);
QString spectrogramParameterKey(const SpectrogramParameters& parameters);

int spectrogramFftSize(int sampleRate);
double spectrogramMaxHz(int sampleRate);
// The centered window starts to light up a little before the hit. Drawing the
// strip later by one eighth of the FFT window (about 6 ms at 44100 Hz) puts
// that onset back on the sound. 44100 Hz is the fallback when the rate is unknown.
inline double spectrogramDisplayDelaySeconds(int sampleRate)
{
    const int rate = sampleRate > 0 ? sampleRate : 44100;
    return static_cast<double>(kSpectrogramFftSize) / (8.0 * static_cast<double>(rate));
}

struct SpectrogramLevel {
    double secondsPerColumn = 0.0;
    int bandCount = 0;
    // column-major: index = column * bandCount + band, band 0 is the lowest frequency.
    QVector<float> magnitudes;

    int columnCount() const;
    double columnsPerSecond() const;
    float magnitudeAt(int column, int band) const;
};

struct SpectrogramData {
    QString normalizedTrackPath;
    qint64 fileSize = -1;
    qint64 lastModifiedMs = -1;
    double durationSeconds = 0.0;
    int sampleRate = 0;
    QVector<SpectrogramLevel> levels;

    bool isEmpty() const;
};

using SpectrogramDataPtr = std::shared_ptr<const SpectrogramData>;

QString spectrogramCacheDirectoryPath(const QString& projectDataDirectoryPath);
QString spectrogramCacheFilePath(
    const QString& normalizedTrackPath,
    const QString& cacheDirectoryPath,
    const SpectrogramParameters& parameters = {});
int bandContainingHz(double hz, int sampleRate);
SpectrogramDataPtr makeSpectrogramPlaceholder(double durationSeconds);
SpectrogramDataPtr buildSpectrogramDataFromSamples(
    const QString& trackPath,
    qint64 fileSize,
    qint64 lastModifiedMs,
    const QVector<float>& samples,
    int sampleRate,
    double durationSeconds,
    const SpectrogramParameters& parameters = {});
SpectrogramDataPtr buildSpectrogramDataFromFile(
    const QString& trackPath,
    qint64 fileSize,
    qint64 lastModifiedMs,
    const miacode::audio_decode::AudioFileDecoder* decoder,
    const SpectrogramParameters& parameters = {});
SpectrogramDataPtr readSpectrogramDataCache(
    const QString& cacheFilePath,
    const QString& normalizedTrackPath,
    qint64 expectedFileSize,
    qint64 expectedLastModifiedMs,
    const SpectrogramParameters& parameters = {});
bool writeSpectrogramDataCache(
    const QString& cacheFilePath,
    const SpectrogramData& data,
    const SpectrogramParameters& parameters = {});
const SpectrogramLevel* selectSpectrogramLevelForVisibleRange(
    const SpectrogramData& data,
    double visibleDurationSeconds,
    int targetPixelWidth);
QPair<int, int> visibleSpectrogramColumnRange(
    const SpectrogramLevel& level,
    double visibleStartSecond,
    double visibleEndSecond);

class SpectrogramCacheService : public QObject {
public:
    using RequestCallback = std::function<void(SpectrogramDataPtr)>;

    explicit SpectrogramCacheService(QObject* parent = nullptr);

    void setThreadPool(QThreadPool* threadPool);
    void setDecoder(std::shared_ptr<const miacode::audio_decode::AudioFileDecoder> decoder);
    void requestSpectrogram(
        const QString& trackPath,
        const QString& cacheDirectoryPath,
        RequestCallback callback,
        const SpectrogramParameters& parameters = {});
    void clear();

private:
    struct PendingRequest {
        QString cacheKey;
        QString trackPath;
        QString normalizedTrackPath;
        QString cacheFilePath;
        qint64 fileSize = -1;
        qint64 lastModifiedMs = -1;
        SpectrogramParameters parameters;
        QVector<RequestCallback> callbacks;
    };

    QString cacheKeyForTrack(
        const QString& trackPath,
        qint64 fileSize,
        qint64 lastModifiedMs,
        const SpectrogramParameters& parameters) const;
    void finishPendingRequest(const std::shared_ptr<PendingRequest>& request, SpectrogramDataPtr data);

    QThreadPool* threadPool_ = nullptr;
    std::shared_ptr<const miacode::audio_decode::AudioFileDecoder> decoder_;
    QHash<QString, SpectrogramDataPtr> memoryCache_;
    QHash<QString, std::shared_ptr<PendingRequest>> pendingRequests_;
};

}  // namespace miacode::spectrogram
