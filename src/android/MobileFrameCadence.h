#pragma once

#include "core/video/PreviewRenderSettings.h"
#include <QtGlobal>
#include <cmath>

namespace miacode::android {

// Independent visual cadence. Missing a deadline drops obsolete frames; it
// never schedules catch-up work or changes the audio/transport clock.
class MobileFrameCadence {
public:
    void configure(PreviewCanvasFrameRateMode mode, double displayHz) {
        const double display = std::isfinite(displayHz) && displayHz >= 1 ? displayHz : 60;
        const double requested = mode == PreviewCanvasFrameRateMode::Fps30 ? 30
            : mode == PreviewCanvasFrameRateMode::Fps120 ? 120
            : mode == PreviewCanvasFrameRateMode::DisplayRefresh ? display : 60;
        intervalNs_ = qMax<qint64>(1, qRound64(1e9 / qMin(display, requested)));
        reset();
    }
    void reset() { nextNs_ = -1; }
    bool due(qint64 nowNs) {
        if (nextNs_ >= 0 && nowNs < nextNs_) return false;
        if (nextNs_ < 0) nextNs_ = nowNs + intervalNs_;
        else nextNs_ += ((nowNs - nextNs_) / intervalNs_ + 1) * intervalNs_;
        return true;
    }
    int pollingIntervalMs() const { return qMax(1, static_cast<int>(intervalNs_ / 1000000)); }
    qint64 intervalNs() const { return intervalNs_; }
private:
    qint64 intervalNs_ = 16666667;
    qint64 nextNs_ = -1;
};
}
