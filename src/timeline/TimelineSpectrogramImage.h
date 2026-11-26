#pragma once

#include <functional>

#include <QColor>

#include "audio/SpectrogramCache.h"
#include "timeline/TimelineSceneState.h"

namespace miacode::timeline {

// Rasterizes the visible spectrogram columns into one premultiplied strip.
// The image is the timeline height in device pixels, so it is not a short
// band texture stretched over the lane. `secondToX` maps an audio second to
// content-space X, using the same mapping as the waveform bars.
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
    qreal devicePixelRatio = 1.0);

}  // namespace miacode::timeline
