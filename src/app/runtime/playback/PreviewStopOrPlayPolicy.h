#pragma once

namespace miacode::runtime {

// Startup can still be cancellable before the transport reports Playing.
inline bool previewStopOrPlayShouldStop(bool playing, bool startupSyncPending,
                                       bool lateVideoStartPending)
{
    return playing || startupSyncPending || lateVideoStartPending;
}

} // namespace miacode::runtime
