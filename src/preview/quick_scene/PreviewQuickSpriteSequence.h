#pragma once

#include "core/scene/PreviewSpriteDescriptor.h"

namespace miacode::preview::quick_scene {

inline constexpr int kPreviewRepeatedSpriteLayerLimit = 32;

struct PreviewSpriteRepetition {
    int period = 0;
    int count = 0;
};

inline bool previewSpritesMatch(
    const scene::PreviewSpriteDescriptor& a, const scene::PreviewSpriteDescriptor& b)
{
    return a.image == b.image && a.center == b.center && a.width == b.width && a.height == b.height
        && a.rotationDegrees == b.rotationDegrees && a.opacity == b.opacity
        && a.sourceRect == b.sourceRect && a.effect == b.effect && a.cacheable == b.cacheable;
}

inline PreviewSpriteRepetition previewSpriteRepetitionAt(
    const scene::PreviewSpriteDescriptors& sprites, int first)
{
    constexpr int minimumRepeats = 1024;
    const int remaining = sprites.size() - first;
    const int maxPeriod = qMin(kPreviewRepeatedSpriteLayerLimit, remaining / minimumRepeats);
    for (int period = 1; period <= maxPeriod; ++period) {
        if (!previewSpritesMatch(sprites[first], sprites[first + period])) continue;
        int end = first + period;
        while (end < sprites.size()
               && previewSpritesMatch(sprites[first + (end - first) % period], sprites[end])) ++end;
        const int count = (end - first) / period;
        if (count >= minimumRepeats) return {period, count};
    }
    return {};
}

} // namespace miacode::preview::quick_scene
