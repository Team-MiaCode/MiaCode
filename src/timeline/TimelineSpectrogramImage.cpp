#include "timeline/TimelineSpectrogramImage.h"

#include <algorithm>
#include <cmath>
#include <functional>

#include <QtMath>

#include "timeline/TimelineThemeConfig.h"

namespace miacode::timeline {

TimelineSpectrogramStrip buildTimelineSpectrogramStrip(
    const miacode::spectrogram::SpectrogramLevel& level,
    double phaseCompensationSeconds,
    double rangeStartSecond,
    double rangeEndSecond,
    qreal timelineTop,
    qreal timelineHeight,
    const QColor& waveformColor,
    double spectrogramOpacity,
    const std::function<qreal(double)>& secondToX,
    qreal devicePixelRatio)
{
    TimelineSpectrogramStrip strip;
    if (level.columnCount() <= 0 || level.bandCount <= 0 || level.secondsPerColumn <= 0.0
        || timelineHeight <= 1.0) {
        return strip;
    }

    const double phase = phaseCompensationSeconds;
    const QPair<int, int> columns = miacode::spectrogram::visibleSpectrogramColumnRange(
        level,
        qMax(0.0, rangeStartSecond + phase),
        qMax(0.0, rangeEndSecond + phase));
    if (columns.second <= columns.first) {
        return strip;
    }

    const double firstSecond =
        level.secondsPerColumn * static_cast<double>(columns.first) - phase;
    const double lastSecond =
        level.secondsPerColumn * static_cast<double>(columns.second) - phase;
    const qreal x0 = secondToX(firstSecond);
    qreal x1 = secondToX(lastSecond);
    if (x1 <= x0) {
        x1 = x0 + 1.0;
    }
    const qreal pixelRatio = qBound<qreal>(1.0, devicePixelRatio, 4.0);
    const int pixelWidth = qBound(1, qRound((x1 - x0) * pixelRatio), 16384);
    const int pixelHeight = qBound(1, qRound(timelineHeight * pixelRatio), 2048);
    QImage image(pixelWidth, pixelHeight, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QVector<float> peaks(pixelWidth * level.bandCount, 0.0f);
    const qreal span = qMax<qreal>(1.0, x1 - x0);
    for (int column = columns.first; column < columns.second; ++column) {
        const double columnStart =
            level.secondsPerColumn * static_cast<double>(column) - phase;
        const qreal columnX0 = secondToX(columnStart);
        qreal columnX1 = secondToX(columnStart + level.secondsPerColumn);
        if (columnX1 <= columnX0) {
            columnX1 = columnX0 + 1.0;
        }
        const int begin = qBound(0, static_cast<int>(std::floor((columnX0 - x0) * pixelWidth / span)), pixelWidth - 1);
        const int end = qBound(begin, static_cast<int>(std::ceil((columnX1 - x0) * pixelWidth / span)) - 1, pixelWidth - 1);
        for (int pixel = begin; pixel <= end; ++pixel) {
            float* destination = peaks.data() + pixel * level.bandCount;
            for (int band = 0; band < level.bandCount; ++band) {
                destination[band] = std::max(destination[band], level.magnitudeAt(column, band));
            }
        }
    }

    Q_UNUSED(waveformColor);
    const float coverage = 1.0f;
    const int alpha = qBound(0, qRound(qBound(0.0, spectrogramOpacity, 1.0) * 255.0), 255);
    const int bandCount = level.bandCount;
    const float bandSpan = static_cast<float>(bandCount - 1);
    constexpr int kHeatStops[5][3] = {
        {8, 12, 24},
        {18, 48, 130},
        {24, 150, 170},
        {242, 168, 42},
        {255, 248, 230},
    };
    for (int row = 0; row < pixelHeight; ++row) {
        auto* line = reinterpret_cast<QRgb*>(image.scanLine(row));
        const float bandPosition = (1.0f - (static_cast<float>(row) + 0.5f) / static_cast<float>(pixelHeight))
            * bandSpan;
        const int band0 = qBound(0, static_cast<int>(std::floor(bandPosition)), bandCount - 1);
        const int band1 = std::min(bandCount - 1, band0 + 1);
        const float fraction = std::min(1.0f, std::max(0.0f, bandPosition - static_cast<float>(band0)));
        const float lowWeight = 1.0f - fraction;
        for (int pixel = 0; pixel < pixelWidth; ++pixel) {
            const float* source = peaks.constData() + pixel * bandCount;
            const float magnitude = source[band0] * lowWeight + source[band1] * fraction;
            const float amount = std::clamp(magnitude * coverage, 0.0f, 1.0f) * 4.0f;
            const int stop = std::min(3, static_cast<int>(std::floor(amount)));
            const float blend = amount - static_cast<float>(stop);
            const int red = qRound(kHeatStops[stop][0] + (kHeatStops[stop + 1][0] - kHeatStops[stop][0]) * blend);
            const int green = qRound(kHeatStops[stop][1] + (kHeatStops[stop + 1][1] - kHeatStops[stop][1]) * blend);
            const int blue = qRound(kHeatStops[stop][2] + (kHeatStops[stop + 1][2] - kHeatStops[stop][2]) * blend);
            line[pixel] = qPremultiply(qRgba(red, green, blue, alpha));
        }
    }

    strip.valid = true;
    strip.rect = QRectF(x0, timelineTop, span, timelineHeight);
    strip.image = image;
    return strip;
}

}  // namespace miacode::timeline
