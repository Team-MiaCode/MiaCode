#include "export/video_export/EncoderProbeCache.h"

#include <mutex>
#include <utility>

namespace miacode::video_export {
namespace {
std::mutex cacheMutex;
std::shared_ptr<EncoderProbeCache> installedCache;
}

void installEncoderProbeCache(std::shared_ptr<EncoderProbeCache> cache)
{
    const std::lock_guard<std::mutex> lock(cacheMutex);
    installedCache = std::move(cache);
}

std::shared_ptr<EncoderProbeCache> encoderProbeCache()
{
    const std::lock_guard<std::mutex> lock(cacheMutex);
    return installedCache;
}

} // namespace miacode::video_export
