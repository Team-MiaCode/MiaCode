#pragma once

#include <QString>
#include <QtGlobal>

namespace miacode::latency {
class LatencySandboxController;
}

namespace miacode {

class LatencyEngine
{
public:
    virtual ~LatencyEngine() = default;

    virtual double documentWholeBpm() const = 0;
    virtual double documentOffsetSeconds() const = 0;
    virtual int documentClockCount() const = 0;
    virtual quint64 documentOpenGeneration() const = 0;
    // Empty when there is no audio to detect against, which is what the page
    // shows its "no track" state for.
    virtual QString trackPath() const = 0;

    virtual void applyDetectorBpm(double bpm) = 0;
    virtual void applyDetectorOffset(double seconds) = 0;
    virtual void applyDetectorClockCount(int clockCount) = 0;

    // The audition sandbox. Null before the window has built one.
    virtual miacode::latency::LatencySandboxController* sandbox() const = 0;

    // Stop any in-progress audition playback (used on file-path change). Only
    // acts while the latency page is selected; never touches normal-difficulty
    // playback. A named intent method rather than routing callers through
    // sandbox() so playback/ never needs to know the sandbox's concrete type.
    virtual void exitSandboxIfActive() = 0;

protected:
    LatencyEngine() = default;
    LatencyEngine(const LatencyEngine&) = default;
    LatencyEngine& operator=(const LatencyEngine&) = default;
};

}  // namespace miacode
