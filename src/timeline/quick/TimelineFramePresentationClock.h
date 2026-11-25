#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>

class TimelineFramePresentationClock
{
public:
    struct FrameSample {
        long long sampledAtNs = -1;
        std::uint64_t generation = 0;
    };

    void beginGuiFrame(bool playing)
    {
        if (playing_.exchange(playing, std::memory_order_acq_rel) != playing) {
            generation_.fetch_add(1, std::memory_order_acq_rel);
        }
        sampledAtNs_.store(playing ? nowNs() : -1, std::memory_order_release);
        if (!playing) leadSeconds_.store(0.0, std::memory_order_release);
    }

    void beginRenderFrame()
    {
        const auto sampledAt = sampledAtNs_.load(std::memory_order_acquire);
        if (sampledAt == lastSampleNs_) return;
        lastSampleNs_ = sampledAt;
        frameSample_ = {sampledAt, generation_.load(std::memory_order_acquire)};
    }

    FrameSample renderFrameSample() const
    {
        if (frameSample_.sampledAtNs >= 0) return frameSample_;
        return {sampledAtNs_.load(std::memory_order_acquire), generation_.load(std::memory_order_acquire)};
    }

    void activateNativePresentation()
    {
        if (!nativePresentation_.exchange(true, std::memory_order_acq_rel)) {
            generation_.fetch_add(1, std::memory_order_acq_rel);
            leadSeconds_.store(0.0, std::memory_order_release);
        }
    }

    void framePresented()
    {
        if (!nativePresentation_.load(std::memory_order_acquire)) {
            recordPresentation(frameSample_, nowNs());
        }
        frameSample_.sampledAtNs = -1;
    }

    // Native presentation callbacks can arrive after pause, resume or window rebinding.
    void recordPresentation(FrameSample sample, long long presentedAtNs)
    {
        if (sample.sampledAtNs < 0 || presentedAtNs < sample.sampledAtNs) return;
        std::lock_guard<std::mutex> guard(samplesMutex_);
        if (!playing_.load(std::memory_order_acquire)
            || sample.generation != generation_.load(std::memory_order_acquire)) return;
        if (sample.generation != samplesGeneration_) {
            samplesGeneration_ = sample.generation;
            sampleCount_ = nextSample_ = 0;
            lastPresentedSampleNs_ = -1;
        }
        if (sample.sampledAtNs <= lastPresentedSampleNs_) return;
        lastPresentedSampleNs_ = sample.sampledAtNs;
        samples_[nextSample_] = double(presentedAtNs - sample.sampledAtNs) / 1000000000.0;
        nextSample_ = (nextSample_ + 1) % samples_.size();
        sampleCount_ = std::min(sampleCount_ + 1, samples_.size());
        if (sampleCount_ < 3) return;
        auto ordered = samples_;
        std::sort(ordered.begin(), ordered.begin() + sampleCount_);
        const auto middle = sampleCount_ / 2;
        const double median = sampleCount_ % 2 == 0
            ? (ordered[middle - 1] + ordered[middle]) * 0.5 : ordered[middle];
        leadSeconds_.store(median, std::memory_order_release);
    }

    double presentationLeadSeconds() const { return leadSeconds_.load(std::memory_order_acquire); }

    static long long nowNs()
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

private:
    std::atomic<long long> sampledAtNs_{-1};
    std::atomic<std::uint64_t> generation_{0};
    std::atomic<bool> playing_{false};
    std::atomic<bool> nativePresentation_{false};
    std::atomic<double> leadSeconds_{0.0};
    FrameSample frameSample_;
    long long lastSampleNs_ = -1;
    std::mutex samplesMutex_;
    std::uint64_t samplesGeneration_ = 0;
    long long lastPresentedSampleNs_ = -1;
    std::array<double, 8> samples_{};
    std::size_t sampleCount_ = 0;
    std::size_t nextSample_ = 0;
};
