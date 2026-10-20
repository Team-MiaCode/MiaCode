#pragma once

#include "audio/PreviewAudioBackend.h"

#include <functional>
#include <memory>

namespace miacode::preview_audio {

using PreviewAudioBackendFactory = std::function<std::unique_ptr<PreviewAudioBackend>()>;

// Outcome of an emergency stop of the active output device, requested from the
// Core Audio callback thread before the worker drains its queued command.
struct BassEmergencyPauseResult {
    bool available = false;
    bool attempted = false;
    bool paused = false;
    int outputDeviceIndex = -1;
    int nativeErrorCode = 0;
    qint64 startedMonotonicNs = 0;
    qint64 finishedMonotonicNs = 0;
};

// The audio backend a host process installs once at startup. MiaCode installs
// the BASS backend from audio/bass; a host without one installs nothing.
struct PreviewAudioBackendProvider {
    PreviewAudioBackendFactory factory;
    // Optional: stops the active output immediately (see BassEmergencyPauseResult).
    std::function<BassEmergencyPauseResult()> pauseActiveOutput;
};

void installPreviewAudioBackendProvider(PreviewAudioBackendProvider provider);

// The installed provider's factory. Empty when no provider is installed; a
// worker then publishes the "backend factory returned null" degraded state.
PreviewAudioBackendFactory productionPreviewAudioBackendFactory();

// Runs the installed provider's emergency pause. Without one it reports an
// unavailable output, which is what an unarmed backend reports.
BassEmergencyPauseResult pauseActivePreviewAudioOutput();

}  // namespace miacode::preview_audio
