#pragma once

#include <mutex>

#include "bass.h"

namespace miacode::audio {

inline bool ensureBassFlacPluginLoaded(int* errorCode = nullptr)
{
    static std::mutex mutex;
    static HPLUGIN plugin = 0;
    std::lock_guard<std::mutex> lock(mutex);
    if (plugin == 0) {
        plugin = BASS_PluginLoad("bassflac", 0);
        if (plugin == 0) {
            if (errorCode != nullptr) {
                *errorCode = static_cast<int>(BASS_ErrorGetCode());
            }
            return false;
        }
    }
    if (errorCode != nullptr) {
        *errorCode = 0;
    }
    return true;
}

} // namespace miacode::audio
