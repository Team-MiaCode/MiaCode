#include "core/scene/PreviewJudgeFireworkLayerState.h"

#include "core/scene/PreviewFireworkWarmupPolicy.h"
#include "core/scene/PreviewSceneConstants.h"
#include "core/scene/PreviewSceneMath.h"

#include <QtMath>
#include <array>

namespace {

constexpr qreal kJudgeEffectFireworkMinSpriteWidthPixels = 1.0;
constexpr qreal kJudgeEffectFireworkTouchTriggerDelaySeconds =
    miacode::preview_gameplay::kJudgeEffectFireworkTouchTriggerDelaySeconds;
constexpr qreal kJudgeEffectFireworkDurationSeconds =
    miacode::preview_gameplay::kJudgeEffectFireworkDurationSeconds;
constexpr qreal kFireworkRotationDegreesPerSecond = 35.0;
constexpr qreal kFireworkFrontLoad = 0.16;

struct ScalarCurveKey {
    qreal time = 0.0;
    qreal value = 0.0;
};

const std::array<ScalarCurveKey, 7> kJudgeEffectFireworkScaleKeys = {{
    {0, 0.02},
    {0.133, 0.36},
    {0.167, 0.72},
    {0.2, 0.93},
    {0.233, 1},
    {0.3, 1.06},
    {1.2, 1.06},
}};

const std::array<ScalarCurveKey, 13> kJudgeEffectFireworkAlphaKeys = {{
    {0, 0},
    {0.133, 0.12},
    {0.167, 0.48},
    {0.2, 0.76},
    {0.233, 0.88},
    {0.3, 0.9},
    {0.867, 0.88},
    {0.9, 0.84},
    {0.967, 0.68},
    {1, 0.47},
    {1.033, 0.25},
    {1.067, 0.1},
    {1.1, 0},
}};

const std::array<ScalarCurveKey, 9> kJudgeEffectFireworkColorBallBigScaleKeys = {{
    {0, 0.44},
    {0.1, 0.445},
    {0.233, 0.457},
    {0.3, 0.48},
    {0.4, 0.485},
    {0.6, 0.5},
    {0.733, 0.51},
    {0.867, 0.518},
    {0.95, 0.52},
}};

const std::array<ScalarCurveKey, 7> kJudgeEffectFireworkColorBallBigAlphaKeys = {{
    {0, 0.95},
    {0.017, 0.70},
    {0.050, 0.44},
    {0.15, 0.42},
    {0.4, 0.32},
    {0.7, 0.18},
    {1.05, 0},
}};

const std::array<ScalarCurveKey, 9> kJudgeEffectFireworkHoleKeys = {{
    {0, -0.5},
    {0.833, -0.5},
    {0.866, -0.24},
    {0.9, 0.06},
    {0.933, 0.26},
    {0.966, 0.5},
    {1, 0.72},
    {1.033, 0.98},
    {1.066, 1.1},
}};

template <std::size_t N>
qreal sampleScalarCurve(const std::array<ScalarCurveKey, N>& keys, qreal time)
{
    if (keys.empty()) {
        return 0.0;
    }
    if (time <= keys.front().time) {
        return keys.front().value;
    }
    if (time >= keys.back().time) {
        return keys.back().value;
    }
    for (std::size_t i = 1; i < keys.size(); ++i) {
        const ScalarCurveKey& next = keys[i];
        if (time > next.time) {
            continue;
        }
        const ScalarCurveKey& prev = keys[i - 1];
        const qreal span = next.time - prev.time;
        if (qFuzzyIsNull(span)) {
            return next.value;
        }
        const qreal t = (time - prev.time) / span;
        return prev.value + (next.value - prev.value) * t;
    }
    return keys.back().value;
}

}  // namespace

namespace miacode::preview::scene {

PreviewJudgeFireworkLayerState buildPreviewJudgeFireworkLayerState(
    const PreviewFrameState& state,
    const PreviewActiveMarkerView& markers,
    const QRectF& playfieldRect
)
{
    PreviewJudgeFireworkLayerState layerState;

    const TimelineNoteMarker* latestTriggerMarker = nullptr;
    qreal latestTriggerSecond = -1.0;
    for (int markerIndex = 0; markerIndex < markers.size(); ++markerIndex) {
        const TimelineNoteMarker& marker = markers.markerAt(markerIndex);
        if (!marker.isFirework) {
            continue;
        }
        if (marker.type != QLatin1String("touch") && marker.type != QLatin1String("touch_hold")) {
            continue;
        }
        if (qFuzzyIsNull(marker.touchPoint.x()) && qFuzzyIsNull(marker.touchPoint.y())) {
            continue;
        }

        const qreal triggerSecond = marker.type == QLatin1String("touch")
            ? static_cast<qreal>(marker.second + kJudgeEffectFireworkTouchTriggerDelaySeconds)
            : static_cast<qreal>(marker.endSecond);
        // At chart start the warm-up is centred at playhead 0, so its deliberate
        // lead makes the synthetic trigger negative. Keep rejecting real
        // negative-time notes while allowing the unique off-screen synthetic.
        if (triggerSecond < 0.0 && !isFireworkWarmupMarker(marker)) {
            continue;
        }

        const qreal elapsedSeconds = static_cast<qreal>(state.playheadSeconds) - triggerSecond;
        if (elapsedSeconds < 0.0 || elapsedSeconds > kJudgeEffectFireworkDurationSeconds) {
            continue;
        }
        if (latestTriggerMarker == nullptr || triggerSecond >= latestTriggerSecond) {
            latestTriggerMarker = &marker;
            latestTriggerSecond = triggerSecond;
        }
    }
    if (latestTriggerMarker == nullptr) {
        return layerState;
    }

    layerState.clipCenter = mapLogicalPointToRect(QPointF(kLogicalCanvasCenter, kLogicalCanvasCenter), playfieldRect);
    const qreal clipRadius = qMax<qreal>(1.0, mapLogicalLengthToRect(kLogicalCanvasCenter, playfieldRect));

    const qreal elapsedSeconds = static_cast<qreal>(state.playheadSeconds) - latestTriggerSecond;
    const qreal elapsedClipTime = qBound<qreal>(0.0, elapsedSeconds, kJudgeEffectFireworkDurationSeconds);
    // Advance the middle of the animation and preserve its start and end times.
    const qreal clipTime = elapsedClipTime + kFireworkFrontLoad * elapsedClipTime
        * (1.0 - elapsedClipTime / kJudgeEffectFireworkDurationSeconds);
    const qreal life01 = qBound<qreal>(0.0, clipTime / kJudgeEffectFireworkDurationSeconds, 1.0);
    const qreal animationTime = miacode::preview_gameplay::judgeEffectFireworkAnimationTime(clipTime);
    const qreal fireworkScale = sampleScalarCurve(kJudgeEffectFireworkScaleKeys, clipTime);
    const qreal fireworkAlpha = sampleScalarCurve(kJudgeEffectFireworkAlphaKeys, clipTime);
    const qreal fireworkRotationDegrees = animationTime * kFireworkRotationDegreesPerSecond;
    const qreal smallGrowth = qBound<qreal>(0.0, (clipTime - 1.0 / 30.0) / (0.333 - 1.0 / 30.0), 1.0);
    const qreal smallRetirement = qBound<qreal>(0.0, clipTime / 0.150, 1.0);
    const qreal colorBallScale = 0.200 + 0.040 * smallGrowth;
    const qreal colorBallBigScale = sampleScalarCurve(kJudgeEffectFireworkColorBallBigScaleKeys, clipTime);
    const qreal colorBallAlpha = 0.90 * (1.0 - smallRetirement * smallRetirement);
    const qreal colorBallBigAlpha = sampleScalarCurve(kJudgeEffectFireworkColorBallBigAlphaKeys, clipTime);
    const qreal holeRadius = clipRadius * sampleScalarCurve(kJudgeEffectFireworkHoleKeys, clipTime);
    const qreal holeMaskRadius = holeRadius + clipRadius * 0.50;
    const bool warmup = isFireworkWarmupMarker(*latestTriggerMarker);
    const QPointF center = warmup ? layerState.clipCenter
        : mapLogicalPointToRect(latestTriggerMarker->touchPoint, playfieldRect);
    // Each burst expands in a fixed circle around its touch position.
    const qreal outerRadius = clipRadius * fireworkScale;
    const qreal colorBallRadius = clipRadius * colorBallScale;
    const qreal colorBallBigRadius = clipRadius * colorBallBigScale;
    const qreal colorBallWidth = colorBallRadius * 2.0;
    const qreal colorBallBigWidth = colorBallBigRadius * 2.0;
    const bool hasColorBallSprite = !state.judgeEffect.fireworkColorBallImage.isNull();
    const bool drawFirework =
        fireworkScale > kRenderDurationEpsilon && fireworkAlpha > kRenderDurationEpsilon;
    const bool drawColorBall = hasColorBallSprite
        && colorBallScale > kRenderDurationEpsilon
        && colorBallAlpha > kRenderDurationEpsilon
        && colorBallWidth > kJudgeEffectFireworkMinSpriteWidthPixels;
    const bool drawColorBallBig = hasColorBallSprite
        && colorBallBigScale > kRenderDurationEpsilon
        && colorBallBigAlpha > kRenderDurationEpsilon
        && colorBallBigWidth > kJudgeEffectFireworkMinSpriteWidthPixels;
    const qreal fallbackColorBallScale = qMax(colorBallScale, colorBallBigScale);
    const qreal fallbackColorBallAlpha = qMax(colorBallAlpha, colorBallBigAlpha);
    const qreal fallbackColorBallRadius =
        clipRadius * fallbackColorBallScale;
    const bool drawFallbackColorBall = !hasColorBallSprite
        && (colorBallScale > kRenderDurationEpsilon || colorBallBigScale > kRenderDurationEpsilon);

    layerState.active = drawFirework
        || drawColorBall
        || drawColorBallBig
        || (drawFallbackColorBall && fallbackColorBallAlpha > kRenderDurationEpsilon);
    if (!layerState.active) {
        return PreviewJudgeFireworkLayerState();
    }

    layerState.drawFirework = drawFirework;
    layerState.warmup = warmup;
    layerState.drawColorBall = drawColorBall;
    layerState.drawColorBallBig = drawColorBallBig;
    layerState.drawFallbackColorBall = drawFallbackColorBall;
    layerState.center = center;
    layerState.clipRadius = clipRadius;
    layerState.triggerSecond = latestTriggerSecond;
    layerState.clipTimeSeconds = clipTime;
    layerState.life01 = life01;
    layerState.fireworkScale = fireworkScale;
    layerState.outerRadius = outerRadius;
    layerState.fireworkAlpha = fireworkAlpha;
    layerState.fireworkBrightness = state.render.fireworkBrightness;
    layerState.fireworkRotationDegrees = fireworkRotationDegrees;
    layerState.holeRadius = holeRadius;
    layerState.holeMaskRadius = holeMaskRadius;
    layerState.colorBallScale = colorBallScale;
    layerState.colorBallBigScale = colorBallBigScale;
    layerState.colorBallAlpha = colorBallAlpha;
    layerState.colorBallBigAlpha = colorBallBigAlpha;
    layerState.colorBallRadius = colorBallRadius;
    layerState.colorBallBigRadius = colorBallBigRadius;
    layerState.fallbackColorBallRadius = fallbackColorBallRadius;
    layerState.fallbackColorBallAlpha = fallbackColorBallAlpha;
    layerState.colorBallImage =
        state.judgeEffect.fireworkColorBallImage.isNull() ? nullptr : &state.judgeEffect.fireworkColorBallImage;
    layerState.colorBallSourceRect = state.judgeEffect.fireworkColorBallSourceRect;
    return layerState;
}

}  // namespace miacode::preview::scene
