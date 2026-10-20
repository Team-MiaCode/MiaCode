#pragma once

#include "audio/PreviewAudioWorkerFactory.h"

#include <QtGlobal>

namespace miacode::preview_audio {

// A tiny process-local control plane for the active Windows BASS output.  The Core
// Audio callback uses it to stop the old endpoint before the audio worker has an
// opportunity to drain a queued command.  Stream lifetime remains worker-owned.
// The result type lives with the backend provider in audio/PreviewAudioWorkerFactory.h.
class PreviewBassEmergencyPause final
{
public:
    // Called only after BASS_Init has successfully bound a concrete output.
    static void arm(int outputDeviceIndex);
    // Called before the backend frees its BASS device/streams, and on teardown.
    static void disarm();
    // Safe for Core Audio's MTA callback thread. It never accesses stream handles.
    static BassEmergencyPauseResult pauseActiveOutput();
};

}  // namespace miacode::preview_audio
