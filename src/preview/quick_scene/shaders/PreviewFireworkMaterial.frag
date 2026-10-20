#version 440

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D source;
layout(binding = 2) uniform sampler2D brightBlockMaskSource;
layout(binding = 3) uniform sampler2D colorBallBigSource;
layout(binding = 4) uniform sampler2D starsSource;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float quadRadius;
    float clipRadius;
    float outerRadius;
    vec4 fireworkAndHole;
    vec4 colorBallSmall;
    vec4 colorBallBig;
    vec4 sourceRect;
    vec4 timing;
    vec4 glowFields[22];
    vec4 starsUvTransform;
};

const float kBrightBlockSpeed = 1.73;
const float kSectorSpanDegrees = 12.0;
const float kSectorStepDegrees = 24.0;
const float kSectorPhaseDegrees = -106.0;
const int kRenderFlagDrawStripe = 0x1;
const int kRenderFlagDrawBigBall = 0x2;
const int kRenderFlagDrawSmallBall = 0x4;
const int kRenderFlagUseTexture = 0x8;
const int kRenderFlagDrawFallbackBall = 0x10;
const int kRenderFlagWarmup = 0x20;

vec4 compositeSourceOver(vec4 sourceColor, vec4 destColor);

vec3 sectorBaseColor(int index)
{
    vec3 color;
    switch (index % 5) {
    case 0:
        color = vec3(255.0, 82.0, 20.0) / 255.0;
        break;
    case 1:
        color = vec3(245.0, 72.0, 148.0) / 255.0;
        break;
    case 2:
        color = vec3(0.0, 162.0, 242.0) / 255.0;
        break;
    case 3:
        color = vec3(186.0, 242.0, 0.0) / 255.0;
        break;
    default:
        color = vec3(255.0, 216.0, 0.0) / 255.0;
        break;
    }
    return color;
}

vec3 cyclingSectorColor(int sectorIndex)
{
    // Reference frames advance one palette slot every 1/9 second, independently of rotation.
    int stepIndex = int(floor(max(0.0, timing.w - 0.003) * 9.0));
    int colorIndex = (sectorIndex + 1 - stepIndex) % 5;
    return sectorBaseColor((colorIndex + 5) % 5);
}

float wrapAngleDegrees(float angle)
{
    float wrapped = mod(angle, 360.0);
    if (wrapped < 0.0) {
        wrapped += 360.0;
    }
    return wrapped;
}

float shortestAngleDistanceDegrees(float angleA, float angleB)
{
    float delta = wrapAngleDegrees(angleA - angleB);
    if (delta > 180.0) {
        delta = 360.0 - delta;
    }
    return abs(delta);
}

float movingBallFieldMask(
    vec2 localPos,
    float centerRadius,
    float trailingLength,
    float leadingLength,
    float playfieldRadius
)
{
    const float frameCount = 12.0;
    vec2 gridSpacing = vec2(
        max(6.0, playfieldRadius * 0.026),
        max(6.0, playfieldRadius * 0.024)
    );
    vec2 gridCell = floor(localPos / gridSpacing + vec2(0.5));
    vec2 ballCenter = gridCell * gridSpacing;
    float ballCenterRadius = length(ballCenter);
    float blockLength = trailingLength + leadingLength;
    float body01 = clamp(
        (ballCenterRadius - (centerRadius - trailingLength)) / max(blockLength, 1.0),
        0.0,
        0.9999
    );
    float radialInside = step(centerRadius - trailingLength, ballCenterRadius)
        * step(ballCenterRadius, centerRadius + leadingLength);
    float frameIndex = floor(body01 * frameCount);
    // The inner of the two peak rows uses an extra-large circle tier.
    float ballRadiusScale = frameIndex == 7.0 ? 1.18 : 1.08;
    vec2 cellUv = (localPos - ballCenter) / (gridSpacing.x * ballRadiusScale) + vec2(0.5);
    float cellInside = step(0.0, cellUv.x) * step(cellUv.x, 1.0)
        * step(0.0, cellUv.y) * step(cellUv.y, 1.0);
    vec2 atlasUv = vec2(cellUv.x, (frameIndex + cellUv.y) / frameCount);
    return texture(brightBlockMaskSource, atlasUv).a * radialInside * cellInside;
}

float brightBlockTravel(float age)
{
    const float deceleration = 0.8;
    return kBrightBlockSpeed * 1.25 * (1.0 - exp(-deceleration * age)) / deceleration;
}

float fireworkSectorCoverage(float angleDegrees, float sectorStartDegrees,
    float radius, float outerRadius, float pixelWidth)
{
    float sectorHalfSpan = kSectorSpanDegrees * 0.5;
    float sectorCenterDegrees = sectorStartDegrees + sectorHalfSpan;
    float angularDistance = shortestAngleDistanceDegrees(angleDegrees, sectorCenterDegrees);
    // Measure the edge in scene pixels; wrapped angles have discontinuous derivatives.
    float edgeDistance = radians(sectorHalfSpan - angularDistance) * radius;
    float angularFeatherPixels = max(pixelWidth, radius * radians(0.35));
    float angularCoverage = smoothstep(-angularFeatherPixels, angularFeatherPixels, edgeDistance);
    float radialCoverage = 1.0 - smoothstep(outerRadius - pixelWidth, outerRadius + pixelWidth, radius);
    return angularCoverage * radialCoverage;
}

vec4 proceduralColorBall(vec2 localRect)
{
    float radial = length((localRect - vec2(0.5)) * 2.0);
    if (radial >= 1.0) {
        return vec4(0.0);
    }

    vec2 point = localRect * 2.0 - 1.0;
    vec3 upper = mix(vec3(1.0, 0.02, 0.88), vec3(0.0, 1.0, 1.0),
        (1.0 - smoothstep(0.0, 0.70, abs(point.x))) * (1.0 - smoothstep(-0.6, 0.3, point.y)));
    vec3 color = mix(upper, vec3(1.0, 0.95, 0.0), smoothstep(0.0, 0.75, point.y));
    float alpha = 1.0 - smoothstep(0.72, 1.0, radial);
    return vec4(color * alpha, alpha);
}

int renderFlagsFromFloat(float flagsValue)
{
    return int(floor(flagsValue + 0.5));
}

bool hasRenderFlag(int renderFlags, int renderFlag)
{
    return (renderFlags & renderFlag) != 0;
}

vec4 compositeSourceOver(vec4 sourceColor, vec4 destColor)
{
    return sourceColor + destColor * (1.0 - sourceColor.a);
}

vec4 sampleColorBallLayer(vec2 localPos, float radius, float alpha, float aspect, bool useTextureBall)
{
    if (radius <= 0.0 || alpha <= 0.0) {
        return vec4(0.0);
    }

    vec2 point = localPos / vec2(radius, max(0.01, radius * aspect));
    float radial = length(point);
    if (radial >= 1.45) {
        return vec4(0.0);
    }
    vec2 localRect = point * 0.5 + vec2(0.5);
    if (any(lessThan(localRect, vec2(0.0))) || any(greaterThan(localRect, vec2(1.0)))) {
        return vec4(0.0);
    }
    vec4 premulColor = useTextureBall
        ? texture(source, sourceRect.xy + localRect * sourceRect.zw)
        : proceduralColorBall(localRect);
    float edge = 1.0 - smoothstep(0.94, 1.0, radial);
    return premulColor * alpha * edge;
}

vec4 sampleSoftColorBallLayer(vec2 localPos, float radius, float alpha, float aspect, bool useTextureBall, float retirementProgress)
{
    if (radius <= 0.0 || alpha <= 0.0) {
        return vec4(0.0);
    }
    vec2 point = localPos / vec2(radius, max(0.01, radius * aspect));
    float radial = length(point);
    float pixelWidth = fwidth(radial);
    float retirement = retirementProgress * retirementProgress;
    float feather = mix(0.016, 0.18, retirement);
    float coverage = 1.0 - smoothstep(0.85 - feather - pixelWidth, 0.85 + feather + pixelWidth, radial);
    if (coverage <= 0.0) {
        return vec4(0.0);
    }
    // Reconstruct solid coverage while sampling the original colour gradient through the edge.
    vec2 samplePoint = localPos * min(1.0, 0.94 / max(radial, 0.001));
    vec4 color;
    if (retirement > 0.0 && useTextureBall) {
        vec2 uv = samplePoint / vec2(radius, radius * aspect) * 0.5 + vec2(0.5);
        float level = log2(1.0 + 12.0 * retirement);
        color = textureLod(colorBallBigSource, uv, level);
    } else {
        color = sampleColorBallLayer(samplePoint, radius, 1.0, aspect, useTextureBall);
    }
    vec3 tint = color.rgb / max(color.a, 0.001);
    return vec4(tint * 0.90 * alpha * coverage, alpha * coverage);
}

vec4 sampleBlurredColorBallLayer(vec2 localPos, float radius, float alpha, float aspect, bool useTextureBall, float blurProgress)
{
    if (radius <= 0.0 || alpha <= 0.0) {
        return vec4(0.0);
    }
    vec2 uv = localPos / vec2(radius, radius * aspect) * 0.5 + vec2(0.5);
    float blur = blurProgress * blurProgress;
    // Mip filtering blurs the complete premultiplied image, including its single alpha edge.
    float level = log2(1.0 + 12.0 * blur);
    vec4 color = useTextureBall ? textureLod(colorBallBigSource, uv, level) : proceduralColorBall(uv);
    float radial = length(uv * 2.0 - 1.0);
    float expansion = smoothstep(0.03, 0.30, timing.x);
    float retirement = smoothstep(0.70, 1.05, timing.x);
    float featherStart = mix(0.84, 0.78, expansion);
    featherStart = mix(featherStart, 0.60, retirement);
    float edgeCoverage = 1.0 - smoothstep(featherStart, 0.98, radial);
    // Retain the colour body while feathering its outer silhouette.
    float bodyCoverage = mix(0.98, 0.90, smoothstep(0.12, 0.65, radial));
    return color * (alpha * bodyCoverage * edgeCoverage);
}

vec4 sampleFirework(vec2 texCoord)
{
    float exitProgress = smoothstep(0.90, 1.10, timing.w + 1.0 / 30.0);
    float exitScale = mix(1.0, 1.045, exitProgress);
    vec2 localPos = (texCoord * 2.0 - 1.0) * quadRadius / exitScale;
    float radius = length(localPos);
    // Evaluate derivatives before radial and sector branches.
    float pixelWidth = max(0.75, 0.5 * length(abs(dFdx(localPos)) + abs(dFdy(localPos))));

    float fireworkAlpha = fireworkAndHole.x;
    float fireworkRotationDegrees = fireworkAndHole.y;
    float holeRadius = fireworkAndHole.z;
    float holeMaskRadius = fireworkAndHole.w;
    float colorBallRadius = colorBallSmall.x;
    float colorBallAlpha = colorBallSmall.y;
    float sourceAspect = max(0.01, colorBallSmall.z);
    float fallbackColorBallRadius = colorBallSmall.w;
    float colorBallBigRadius = colorBallBig.x;
    float colorBallBigAlpha = colorBallBig.y;
    float fallbackColorBallAlpha = colorBallBig.z;
    int renderFlags = renderFlagsFromFloat(colorBallBig.w);
    if (hasRenderFlag(renderFlags, kRenderFlagWarmup)) {
        return vec4(0.0);
    }
    bool drawFirework = hasRenderFlag(renderFlags, kRenderFlagDrawStripe);
    bool drawColorBallBig = hasRenderFlag(renderFlags, kRenderFlagDrawBigBall);
    bool drawColorBallSmall = hasRenderFlag(renderFlags, kRenderFlagDrawSmallBall);
    bool useTextureBall = hasRenderFlag(renderFlags, kRenderFlagUseTexture);
    bool drawFallbackColorBall = hasRenderFlag(renderFlags, kRenderFlagDrawFallbackBall);

    float holeMask = 1.0;
    if (holeMaskRadius > 0.0) {
        holeMask = smoothstep(holeRadius, holeMaskRadius, radius);
    }

    vec4 rayLayer = vec4(0.0);
    if (drawFirework && radius <= outerRadius + pixelWidth && fireworkAlpha > 0.0) {
        float angleDegrees = wrapAngleDegrees(degrees(atan(localPos.y, localPos.x)));
        float fireworkContributionAlpha = clamp(fireworkAlpha / 0.90, 0.0, 1.0);
        // Fifteen evenly spaced sectors cover the circle. Locate the nearest
        // sector from the angle instead of checking all fifteen per pixel.
        float sectorPhase = wrapAngleDegrees(
            angleDegrees - kSectorPhaseDegrees - fireworkRotationDegrees);
        int sectorIndex = int(floor((sectorPhase + (kSectorStepDegrees - kSectorSpanDegrees) * 0.5) / kSectorStepDegrees)) % 15;
        float sectorStart = kSectorPhaseDegrees + fireworkRotationDegrees + float(sectorIndex) * kSectorStepDegrees;
        float sectorCoverage = fireworkSectorCoverage(angleDegrees, sectorStart, radius, outerRadius, pixelWidth);
        if (sectorCoverage > 0.0) {
            // Opacity travels outward behind the expanding edge, ahead of the full dwell.
            float fieldDistance = clamp(radius / clipRadius, 0.0, 1.0);
            float arrivalSecond = 0.065 + fieldDistance * 0.065;
            float launchAlpha = smoothstep(arrivalSecond, arrivalSecond + 0.05, timing.w);
            // The travelling head sharpens before settling into the outer edge feather.
            float headProgress = smoothstep(0.36, 1.0, outerRadius / clipRadius);
            float headFeather = mix(0.24, 0.20, headProgress);
            float edgeArrival = smoothstep(1.0, 1.06, outerRadius / clipRadius);
            float headFade = smoothstep(1.0 - headFeather, 1.01, radius / outerRadius);
            float frontAlpha = 1.0 - headFade * (1.0 - edgeArrival);
            float sectorAlpha = fireworkContributionAlpha * sectorCoverage
                * launchAlpha * frontAlpha * 0.74;
            float sectorCenter = sectorStart + kSectorSpanDegrees * 0.5;
            float angularDistance = shortestAngleDistanceDegrees(angleDegrees, sectorCenter);
            float axial01 = 1.0 - smoothstep(0.0, kSectorSpanDegrees * 0.5, angularDistance);
            float blockTrailingLength = clipRadius * 0.25;
            float blockLeadingLength = clipRadius * 0.14;
            float blockOutwardExtension = clipRadius * 0.055;
            // Both waves start at the root and follow the same decelerating travel curve.
            float firstFront = brightBlockTravel(max(0.0, timing.w - (0.10 - 5.0 / 30.0)));
            float secondFront = min(0.78, brightBlockTravel(max(0.0, timing.w - (0.43 - 1.0 / 30.0))));
            float firstBlockCenter = clipRadius * firstFront - blockLeadingLength;
            float secondBlockCenter = clipRadius * secondFront - blockLeadingLength;
            float firstBlock = movingBallFieldMask(
                localPos,
                firstBlockCenter,
                blockTrailingLength,
                blockLeadingLength + min(blockOutwardExtension, clipRadius * firstFront),
                clipRadius
            ) * smoothstep(0.10 - 5.0 / 30.0, 0.12 - 5.0 / 30.0, timing.w);
            float secondBlock = movingBallFieldMask(
                localPos,
                secondBlockCenter,
                blockTrailingLength,
                blockLeadingLength + min(blockOutwardExtension, clipRadius * secondFront),
                clipRadius
            ) * smoothstep(0.43 - 1.0 / 30.0, 0.45 - 1.0 / 30.0, timing.w);
            float brightBlockLight = max(firstBlock, secondBlock);
            float colorRise = smoothstep(0.16, 0.34, timing.x);
            float saturation = mix(0.96, 1.03, colorRise);
            float radialTint = smoothstep(0.05, 1.0, fieldDistance);
            float brightness = 0.78;
            brightness *= mix(0.82, 1.12, radialTint);
            brightness *= mix(0.90, 1.04, axial01);
            brightness *= mix(0.66, 1.12, brightBlockLight);
            vec3 baseColor = cyclingSectorColor(sectorIndex);
            float baseLuma = dot(baseColor, vec3(0.2126, 0.7152, 0.0722));
            vec3 beamColor = mix(vec3(baseLuma), baseColor, saturation);
            float outerLightening = baseColor.b > baseColor.r ? 0.02 : 0.07;
            beamColor = mix(beamColor, vec3(1.0), radialTint * outerLightening);
            beamColor = clamp(beamColor * brightness, vec3(0.0), vec3(1.0));
            vec4 fireworkLayer = vec4(
                beamColor * sectorAlpha,
                sectorAlpha
            );
            rayLayer = compositeSourceOver(fireworkLayer, rayLayer);
        }
    }

    float clipTime = timing.x;
    // Keep the spokes connected during the burst, then open their centre as they fade.
    float burstEdge = 1.0 - smoothstep(clipRadius * 0.80, clipRadius, radius);
    vec4 layerColor = rayLayer * holeMask * burstEdge;
    vec3 washEmission = vec3(0.0);

    if (drawFirework && outerRadius > 0.0 && fireworkAlpha > 0.0) {
        // The neutral wash sits behind the coloured emission on its own radial curve.
        float washTime = timing.w + 1.0 / 30.0;
        float washSpread = smoothstep(0.09, 0.32, washTime);
        float washRadius = clipRadius * mix(0.42, 1.08, washSpread);
        float washFeather = clipRadius * mix(0.32, 0.18, washSpread);
        float washEdge = 1.0 - smoothstep(washRadius - washFeather, washRadius + washFeather, radius);
        float washRetirement = smoothstep(0.86, 1.10, washTime);
        float washClearRadius = clipRadius * mix(-0.30, 1.30, washRetirement);
        float washClear = smoothstep(washClearRadius - clipRadius * 0.26,
            washClearRadius + clipRadius * 0.26, radius);
        float washAlpha = 0.14 * fireworkAlpha * washEdge * washClear;
        washEmission = vec3(0.86, 0.88, 0.91) * washAlpha;
    }

    vec4 rootLayer = vec4(0.0);
    float lightRetirement = clamp((timing.w + 1.0 / 30.0 - 0.567) / 0.433, 0.0, 1.0);
    float lightReturn = lightRetirement * lightRetirement;
    float lightShrink = 1.0 - lightReturn;

    if (drawFirework && fireworkAlpha > 0.0 && lightShrink > 0.0) {
        // Independent light emission: soft radiance with no filled spike boundary.
        float burstProgress = clamp(timing.x / 0.100, 0.0, 1.0);
        float outward = burstProgress * burstProgress * (3.0 - 2.0 * burstProgress);
        float roundness = 1.0 - pow(lightRetirement, 4.0);
        float dispersion = outward * roundness;
        vec2 lightPoint = localPos / (clipRadius * lightShrink * 0.75);
        float coreWidth = mix(0.026, 0.054, dispersion);
        float density = exp(-dot(lightPoint, lightPoint) / (coreWidth * coreWidth));
        // Elliptical Gaussian fields merge into one light, then collapse into the round core.
        if (radius < clipRadius * 0.34) {
            for (int ray = 0; ray < 11; ++ray) {
                vec4 axes = glowFields[ray * 2];
                vec4 shape = glowFields[ray * 2 + 1];
                vec2 relative = lightPoint - shape.xy;
                vec2 fieldPoint = vec2(dot(relative, axes.xy) * axes.z,
                    dot(relative, vec2(-axes.y, axes.x)) * axes.w);
                float rayDensity = shape.z
                    * exp(-2.0 * (pow(abs(fieldPoint.x), 2.4) + fieldPoint.y * fieldPoint.y));
                // Weight the join by shared radiance so empty fields contribute zero light.
                float blend = max(0.12 - abs(density - rayDensity), 0.0) / 0.12;
                float sharedRadiance = clamp(min(density, rayDensity) / 0.12, 0.0, 1.0);
                density = max(density, rayDensity) + blend * blend * 0.03 * sharedRadiance;
            }
        }
        // Continuous radiance gives the beams a bright root and soft longitudinal falloff.
        float glowAlpha = pow(clamp(density, 0.0, 1.0), mix(2.8, 1.15, dispersion));
        vec3 glowColor = vec3(1.0, 0.93, 0.55);
        rootLayer = vec4(glowColor * glowAlpha, glowAlpha);

    }

    vec4 stars = texture(starsSource, texCoord * starsUvTransform.zw + starsUvTransform.xy);
    layerColor = compositeSourceOver(stars, layerColor);

    if (drawColorBallBig) {
        float elapsed = timing.w + 1.0 / 30.0;
        float haloRetirement = smoothstep(0.10, 0.32, elapsed);
        float haloRadius = clipRadius * 1.12 * mix(0.44, 0.429, smoothstep(0.0, 0.10, elapsed))
            * mix(1.0, 1.06, haloRetirement);
        float haloOpacity = smoothstep(0.0, 0.033, elapsed) * 0.95 * (1.0 - haloRetirement);
        if (haloOpacity > 0.0 && radius > haloRadius * 0.60) {
            vec2 haloPoint = localPos / max(radius, 1.0) * haloRadius * 0.72;
            vec4 haloSample = sampleColorBallLayer(
                haloPoint, haloRadius, 1.0, sourceAspect, useTextureBall);
            vec3 haloTint = haloSample.rgb / max(haloSample.a, 0.001);
            haloTint = mix(haloTint, vec3(1.0), 0.15);
            float haloDistance = max(0.0, radius / haloRadius - 0.70) / 0.62;
            float haloAlpha = exp(-2.0 * haloDistance * haloDistance)
                * smoothstep(0.60, 0.72, radius / haloRadius)
                * haloOpacity * 0.42;
            float combinedAlpha = haloAlpha + layerColor.a * (1.0 - haloAlpha);
            layerColor = vec4(min(layerColor.rgb + haloTint * haloAlpha, vec3(combinedAlpha)), combinedAlpha);
        }
        vec4 ball = sampleBlurredColorBallLayer(
            localPos, colorBallBigRadius, min(1.0, colorBallBigAlpha * 1.08), sourceAspect, useTextureBall,
            mix(0.20, 1.0, smoothstep(0.72, 0.88, elapsed)));
        float illumination = smoothstep(0.13, 0.267, timing.w);
        vec3 emittedLight = ball.rgb * mix(1.0, 1.50, illumination);
        // Light strength and image opacity remain independent during dissolution.
        vec3 litColor = min(layerColor.rgb + emittedLight, vec3(1.0));
        float combinedAlpha = ball.a + layerColor.a * (1.0 - ball.a);
        vec4 illuminatedBall = vec4(min(litColor, vec3(combinedAlpha)), combinedAlpha);
        layerColor = mix(compositeSourceOver(ball, layerColor), illuminatedBall, illumination);

    }
    if (drawColorBallSmall) {
        float retirement = clamp(timing.x / 0.150, 0.0, 1.0);
        vec4 ball = sampleSoftColorBallLayer(
            localPos, colorBallRadius, colorBallAlpha, sourceAspect, useTextureBall, retirement);
        layerColor = compositeSourceOver(ball, layerColor);
    }
    if (drawFallbackColorBall) {
        layerColor = compositeSourceOver(
            sampleColorBallLayer(localPos, fallbackColorBallRadius, fallbackColorBallAlpha, 1.0, false),
            layerColor
        );
    }

    // Core coverage removes the underlying tint; emission keeps its own warm colour.
    layerColor.rgb += washEmission * (1.0 - layerColor.a);
    float coreCoverage = clamp(rootLayer.a * 1.10, 0.0, 1.0);
    vec3 coreEmission = rootLayer.rgb * 1.10;
    layerColor = vec4(
        coreEmission + layerColor.rgb * (1.0 - coreCoverage),
        coreCoverage + layerColor.a * (1.0 - coreCoverage));


    float fieldEdge = 1.0 - 0.42 * smoothstep(clipRadius * 0.98, clipRadius * 1.06, radius);
    return layerColor * fieldEdge;
}

void main()
{
    fragColor = sampleFirework(vTexCoord) * qt_Opacity;
}
