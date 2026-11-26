#include <cmath>

#include <QDir>
#include <QString>
#include <QTemporaryDir>
#include <QTextStream>
#include <QVector>
#include <QtMath>

#include "audio/SpectrogramCache.h"

namespace {

bool require(bool condition, const QString& message, QTextStream& err)
{
    if (!condition) {
        err << "FAIL: " << message << Qt::endl;
    }
    return condition;
}

float peakInBand(const miacode::spectrogram::SpectrogramLevel& level, int band)
{
    float peak = 0.0f;
    for (int column = 0; column < level.columnCount(); ++column) {
        peak = std::max(peak, level.magnitudeAt(column, band));
    }
    return peak;
}

}  // namespace

int main()
{
    QTextStream err(stderr);
    QTextStream out(stdout);
    bool ok = true;

    constexpr int sampleRate = 44100;
    constexpr double durationSeconds = 1.0;
    constexpr double toneHz = 440.0;
    QVector<float> samples(sampleRate);
    for (int index = 0; index < samples.size(); ++index) {
        const double phase = 2.0 * 3.14159265358979323846 * toneHz
            * static_cast<double>(index) / static_cast<double>(sampleRate);
        samples[index] = static_cast<float>(std::sin(phase));
    }

    const miacode::spectrogram::SpectrogramDataPtr data =
        miacode::spectrogram::buildSpectrogramDataFromSamples(
            QStringLiteral("tone.wav"),
            1,
            2,
            samples,
            sampleRate,
            durationSeconds);
    ok &= require(data != nullptr && !data->levels.isEmpty(),
                  QStringLiteral("a one-second sine produces a spectrogram pyramid"), err);
    if (data == nullptr || data->levels.isEmpty()) {
        err << "FAIL" << Qt::endl;
        return 1;
    }

    const miacode::spectrogram::SpectrogramLevel& top = data->levels.constFirst();
    ok &= require(top.bandCount == miacode::spectrogram::kSpectrogramBandCount
                      && top.columnCount() == qRound(durationSeconds * miacode::spectrogram::kSpectrogramColumnsPerSecond),
                  QStringLiteral("the top level has 256 bands and 192 columns per second"), err);

    int peakBand = 0;
    float peak = -1.0f;
    for (int band = 0; band < top.bandCount; ++band) {
        const float bandPeak = peakInBand(top, band);
        if (bandPeak > peak) {
            peak = bandPeak;
            peakBand = band;
        }
    }
    const int expectedBand = miacode::spectrogram::bandContainingHz(toneHz, sampleRate);
    const int highBand = miacode::spectrogram::bandContainingHz(3000.0, sampleRate);
    ok &= require(qAbs(peakBand - expectedBand) <= 1,
                  QStringLiteral("the 440 Hz sine peaks in the band that contains 440 Hz"), err);
    ok &= require(peak > 0.5f && peak > peakInBand(top, highBand) + 0.4f,
                  QStringLiteral("the 440 Hz peak is louder than a 3 kHz band"), err);

    for (int levelIndex = 1; levelIndex < data->levels.size(); ++levelIndex) {
        const miacode::spectrogram::SpectrogramLevel& previous = data->levels.at(levelIndex - 1);
        const miacode::spectrogram::SpectrogramLevel& level = data->levels.at(levelIndex);
        ok &= require(qAbs(level.secondsPerColumn - previous.secondsPerColumn * 2.0) < 1.0e-9,
                      QStringLiteral("coarser levels double the seconds covered by one column"), err);
    }
    ok &= require(data->levels.constLast().columnCount() == 1,
                  QStringLiteral("the pyramid ends at a single column"), err);

    QTemporaryDir cacheDir;
    ok &= require(cacheDir.isValid(), QStringLiteral("a temporary cache directory is available"), err);
    if (cacheDir.isValid()) {
        const QString cacheFilePath = miacode::spectrogram::spectrogramCacheFilePath(
            data->normalizedTrackPath, cacheDir.path());
        ok &= require(miacode::spectrogram::writeSpectrogramDataCache(cacheFilePath, *data),
                      QStringLiteral("the spectrogram cache file is written"), err);
        const miacode::spectrogram::SpectrogramDataPtr restored =
            miacode::spectrogram::readSpectrogramDataCache(
                cacheFilePath, data->normalizedTrackPath, data->fileSize, data->lastModifiedMs);
        ok &= require(restored != nullptr
                          && restored->levels.size() == data->levels.size()
                          && restored->durationSeconds == data->durationSeconds
                          && !restored->levels.isEmpty()
                          && qAbs(peakInBand(restored->levels.constFirst(), peakBand) - peak) < 1.0e-6f,
                      QStringLiteral("reading the cache restores the peak band"), err);
    }

    if (ok) {
        out << "PASS" << Qt::endl;
        return 0;
    }
    err << "FAIL" << Qt::endl;
    return 1;
}
