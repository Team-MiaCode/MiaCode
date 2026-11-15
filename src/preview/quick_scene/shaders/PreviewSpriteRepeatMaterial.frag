#version 440

layout(location = 0) in vec2 vPosition;
layout(location = 0) out vec4 fragColor;
layout(binding = 1) uniform sampler2D source;

struct SpriteLayer {
    vec4 rect;
    vec4 uv;
    vec4 transform;
    vec4 uvClamp;
};

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float wave;
    float absWave;
    float effect;
    vec4 counts;
    SpriteLayer layers[32];
};

vec4 sampleLayer(SpriteLayer layer)
{
    vec2 delta = vPosition - layer.rect.xy;
    float c = layer.transform.x;
    float s = layer.transform.y;
    vec2 position = vec2(c * delta.x + s * delta.y, -s * delta.x + c * delta.y) * layer.rect.zw + 0.5;
    vec2 uv = layer.uv.xy + position * layer.uv.zw;
    vec2 r = fwidth(uv) * 0.25;
    vec4 texel = (
        texture(source, clamp(uv + vec2(r.x, r.y), layer.uvClamp.xy, layer.uvClamp.zw))
        + texture(source, clamp(uv + vec2(-r.x, r.y), layer.uvClamp.xy, layer.uvClamp.zw))
        + texture(source, clamp(uv + vec2(r.x, -r.y), layer.uvClamp.xy, layer.uvClamp.zw))
        + texture(source, clamp(uv + vec2(-r.x, -r.y), layer.uvClamp.xy, layer.uvClamp.zw))) * 0.25;
    if (position.x < 0.0 || position.y < 0.0 || position.x >= 1.0 || position.y >= 1.0 || texel.a <= 0.0) return vec4(0.0);
    vec3 color = texel.rgb / texel.a;
    if (layer.transform.w > 1.5) {
        color *= 0.95 + max(wave * 0.65, 0.0);
        color = vec3(0.5) + (color - vec3(0.5)) * (1.0 + min(wave * -0.55, 0.0));
    } else if (layer.transform.w > 0.5) {
        color *= 0.95 + absWave * 0.5;
    }
    return vec4(clamp(color, 0.0, 1.0) * texel.a, texel.a) * layer.transform.z * qt_Opacity;
}

void main()
{
    vec4 texels[32];
    int activeCount = 0;
    for (int index = 0; index < int(counts.x); ++index) {
        vec4 texel = sampleLayer(layers[index]);
        if (texel.a > 0.0) texels[activeCount++] = texel;
    }
    // Match the RGBA8 target's rounding after each source-over operation.
    // Iterating a monotone map on four 8-bit channels reaches a fixed point
    // within 1020 changes; remaining copies then have the same result.
    // The two passes compute the fixed-point interval from both ends, then
    // clamp the existing framebuffer with MAX / MIN blending. This retains
    // the RGBA8 rounding even over video and other nontransparent layers.
    vec4 result = counts.z > 0.5 ? vec4(1.0) : vec4(0.0);
    for (int repeat = 0; repeat < min(int(counts.y), 1024); ++repeat) {
        vec4 previous = result;
        for (int index = 0; index < activeCount; ++index) {
            vec4 texel = texels[index];
            result = floor(clamp(texel + result * (1.0 - texel.a), 0.0, 1.0) * 255.0 + 0.5) / 255.0;
        }
        if (all(equal(result, previous))) break;
    }
    fragColor = result;
}
