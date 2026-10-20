#pragma once

#include <QString>
#include <memory>

namespace miacode::video_export {

// Machine runtime cache supplied by the host. Export remains usable without it.
class EncoderProbeCache {
public:
    virtual ~EncoderProbeCache() = default;
    virtual QString preferredHardwareEncoder() const = 0;
    virtual void rememberPreferredHardwareEncoder(const QString& codec) = 0;
};

// Shared ownership covers calls already in progress when a host replaces it.
void installEncoderProbeCache(std::shared_ptr<EncoderProbeCache> cache);
std::shared_ptr<EncoderProbeCache> encoderProbeCache();

} // namespace miacode::video_export
