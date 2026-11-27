#pragma once

#include <QtGlobal>

#include <algorithm>
#include <array>
#include <chrono>
#include <functional>

namespace miacode::preview::scene {

struct PreviewFrameMetrics {
    double fps = 0.0;
    double avgFrameMs = 0.0;
    double maxFrameMs = 0.0;
    double low1Fps = 0.0;
    int sampleCount = 0;
    bool active = false;

    bool operator==(const PreviewFrameMetrics&) const = default;
};

class PreviewFrameRateMonitor {
public:
    static constexpr int kCapacity = 512;
    static constexpr qint64 kWindowNs = 1'000'000'000LL;
    static constexpr qint64 kDiscontinuityNs = 500'000'000LL;
    static constexpr qint64 kIdleTimeoutNs = 500'000'000LL;

    static qint64 steadyNowNs()
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    void reset()
    {
        head_ = 0;
        count_ = 0;
        lastTimestampNs_ = 0;
        lastActiveAvgFrameMs_ = 0.0;
    }

    void markDiscontinuity()
    {
        lastTimestampNs_ = 0;
    }

    void record(qint64 timestampNs)
    {
        if (timestampNs <= 0 || (lastTimestampNs_ > 0 && timestampNs <= lastTimestampNs_)) {
            return;
        }
        const bool startNewRun =
            (lastTimestampNs_ <= 0) || ((timestampNs - lastTimestampNs_) > kDiscontinuityNs);
        if (startNewRun) {
            head_ = 0;
            count_ = 1;
            samples_[0] = Sample{timestampNs, 0.0};
            lastTimestampNs_ = timestampNs;
            return;
        }

        const double intervalMs = static_cast<double>(timestampNs - lastTimestampNs_) / 1'000'000.0;
        pushSample(Sample{timestampNs, intervalMs});
        lastTimestampNs_ = timestampNs;

        const qint64 cutoffNs = timestampNs - kWindowNs;
        while (count_ > 1 && samples_[head_].timestampNs < cutoffNs) {
            popFront();
        }

        double sumMs = 0.0;
        int validCount = 0;
        for (int i = 0; i < count_; ++i) {
            const Sample& sample = sampleAt(i);
            if (sample.intervalMs > 0.0) {
                sumMs += sample.intervalMs;
                ++validCount;
            }
        }
        if (validCount > 0) {
            lastActiveAvgFrameMs_ = sumMs / static_cast<double>(validCount);
        }
    }

    PreviewFrameMetrics snapshot(qint64 nowNs) const
    {
        if (count_ == 0 || lastTimestampNs_ <= 0 || (nowNs - lastTimestampNs_) > kIdleTimeoutNs) {
            return {};
        }

        std::array<double, kCapacity> intervals{};
        int validCount = 0;
        double sumMs = 0.0;
        double maxMs = 0.0;
        qint64 newestTs = 0;
        const qint64 cutoffNs = nowNs - kWindowNs;
        for (int i = 0; i < count_; ++i) {
            const Sample& sample = sampleAt(i);
            if (sample.timestampNs < cutoffNs || sample.intervalMs <= 0.0) {
                continue;
            }
            intervals[validCount++] = sample.intervalMs;
            sumMs += sample.intervalMs;
            maxMs = qMax(maxMs, sample.intervalMs);
            newestTs = qMax(newestTs, sample.timestampNs);
        }
        if (validCount == 0) {
            return {};
        }

        const double meanMs = sumMs / static_cast<double>(validCount);
        const double trailingGapMs = newestTs > 0
            ? qMax(0.0, static_cast<double>(nowNs - newestTs) / 1'000'000.0)
            : 0.0;
        const bool includeTrailing = trailingGapMs > (meanMs * 1.5);
        int lowSampleTotal = validCount;
        double effectiveAvgMs = meanMs;
        double effectiveMaxMs = maxMs;
        if (includeTrailing) {
            effectiveAvgMs = (sumMs + trailingGapMs) / static_cast<double>(validCount + 1);
            effectiveMaxMs = qMax(maxMs, trailingGapMs);
            if (lowSampleTotal < kCapacity) {
                intervals[lowSampleTotal++] = trailingGapMs;
            }
        }

        const int lowCount = qMax(1, (lowSampleTotal + 99) / 100);
        std::nth_element(
            intervals.begin(),
            intervals.begin() + lowCount,
            intervals.begin() + lowSampleTotal,
            std::greater<double>());
        double lowSumMs = 0.0;
        for (int i = 0; i < lowCount; ++i) {
            lowSumMs += intervals[i];
        }
        const double lowAvgMs = lowSumMs / static_cast<double>(lowCount);

        PreviewFrameMetrics metrics;
        metrics.active = true;
        metrics.sampleCount = validCount;
        metrics.avgFrameMs = effectiveAvgMs;
        metrics.fps = effectiveAvgMs > 1e-6 ? 1000.0 / effectiveAvgMs : 0.0;
        metrics.maxFrameMs = effectiveMaxMs;
        metrics.low1Fps = lowAvgMs > 1e-6 ? 1000.0 / lowAvgMs : 0.0;
        return metrics;
    }

    double lastActiveAvgFrameMs() const
    {
        return lastActiveAvgFrameMs_;
    }

private:
    struct Sample {
        qint64 timestampNs = 0;
        double intervalMs = 0.0;
    };

    const Sample& sampleAt(int offset) const
    {
        return samples_[(head_ + offset) % kCapacity];
    }

    void pushSample(const Sample& sample)
    {
        if (count_ == kCapacity) {
            head_ = (head_ + 1) % kCapacity;
            --count_;
        }
        samples_[(head_ + count_) % kCapacity] = sample;
        ++count_;
    }

    void popFront()
    {
        if (count_ > 0) {
            head_ = (head_ + 1) % kCapacity;
            --count_;
        }
    }

    std::array<Sample, kCapacity> samples_{};
    int head_ = 0;
    int count_ = 0;
    qint64 lastTimestampNs_ = 0;
    double lastActiveAvgFrameMs_ = 0.0;
};

}  // namespace miacode::preview::scene
