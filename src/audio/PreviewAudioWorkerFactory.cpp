#include "audio/PreviewAudioWorkerFactory.h"

#include <chrono>
#include <mutex>
#include <utility>

namespace miacode::preview_audio {
namespace {

std::mutex& providerMutex()
{
    static std::mutex mutex;
    return mutex;
}

PreviewAudioBackendProvider& installedProvider()
{
    static PreviewAudioBackendProvider provider;
    return provider;
}

qint64 steadyNowNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

}  // namespace

void installPreviewAudioBackendProvider(PreviewAudioBackendProvider provider)
{
    const std::lock_guard lock(providerMutex());
    installedProvider() = std::move(provider);
}

PreviewAudioBackendFactory productionPreviewAudioBackendFactory()
{
    const std::lock_guard lock(providerMutex());
    return installedProvider().factory;
}

BassEmergencyPauseResult pauseActivePreviewAudioOutput()
{
    std::function<BassEmergencyPauseResult()> pause;
    {
        const std::lock_guard lock(providerMutex());
        pause = installedProvider().pauseActiveOutput;
    }
    if (pause) {
        return pause();
    }
    BassEmergencyPauseResult result;
    result.startedMonotonicNs = steadyNowNs();
    result.finishedMonotonicNs = result.startedMonotonicNs;
    return result;
}

}  // namespace miacode::preview_audio
