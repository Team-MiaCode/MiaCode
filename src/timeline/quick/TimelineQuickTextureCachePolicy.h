#pragma once

#include <QtGlobal>

namespace miacode::timeline::quick {

// Capacity policy for TimelineQuickTextureCache.
//
// Deliberately a SEPARATE predicate from
// preview/quick_scene/PreviewTextureGenerationPolicy.h rather than a shared one:
// the timeline surface and the QSG preview are independent renderers that must not
// include each other's headers, and their pressure is not the same shape. The
// preview caches a small set of large sprite atlases, so bytes bind first. The
// timeline caches many tiny textures whose key space explodes on COUNT — slide-track
// arrow rotation is derived from on-screen pixel deltas via qAtan2 and quantised to
// 0.1 degrees, so every chart contributes a fresh batch of keys. A measured 10-switch
// session grew the rotation key set by ~30 per switch with no plateau, at roughly
// 3.5 KB per entry — i.e. a byte cap alone would never have fired.
//
constexpr bool timelineTextureCacheFlushRequired(
    qsizetype textureCount,
    qint64 textureBytes,
    qsizetype textureEntryLimit,
    qint64 textureByteLimit
)
{
    return textureCount > textureEntryLimit
        || textureBytes > textureByteLimit;
}

}  // namespace miacode::timeline::quick
