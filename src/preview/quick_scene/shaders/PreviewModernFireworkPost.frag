#version 440
layout(location = 0) in vec2 uv;
layout(location = 1) in vec4 tint;
layout(location = 2) in vec2 scenePosition;
layout(location = 0) out vec4 fragColor;
layout(binding = 1) uniform sampler2D source;
layout(binding = 2) uniform sampler2D bloomSource;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float dx;
    float dy;
    float pass;
    vec4 edgeMask;
};
vec4 brightPass(vec4 color) {
    float brightness = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
    return vec4(color.rgb * smoothstep(0.55, 0.90, brightness), 0.0);
}
void main() {
    vec4 center = texture(source, uv);
    if (pass > 0.5) {
        vec2 offset = pass < 1.5 ? vec2(dx, 0.0) : vec2(0.0, dy);
        vec4 low = texture(source, uv - offset);
        vec4 high = texture(source, uv + offset);
        if (pass < 1.5) {
            center = brightPass(center); low = brightPass(low); high = brightPass(high);
        }
        fragColor = (center * 0.5 + (low + high) * 0.25) * qt_Opacity;
    } else {
        // Edge softening uses a subpixel footprint, independently of bloom spread.
        vec2 softnessOffset = fwidth(uv) * 0.65;
        vec4 neighbours = texture(source, uv - vec2(softnessOffset.x, 0.0))
            + texture(source, uv + vec2(softnessOffset.x, 0.0))
            + texture(source, uv - vec2(0.0, softnessOffset.y))
            + texture(source, uv + vec2(0.0, softnessOffset.y));
        vec4 softened = mix(center, (center * 4.0 + neighbours) / 8.0, 0.18);
        vec3 bloom = texture(bloomSource, uv).rgb * 0.08;
        float bloomAlpha = clamp(max(bloom.r, max(bloom.g, bloom.b)), 0.0, 1.0);
        float alpha = softened.a + bloomAlpha * (1.0 - softened.a);
        // Emission can exceed coverage; retain it through source-over blending.
        float edgeCoverage = edgeMask.w > 0.0
            ? 1.0 - smoothstep(edgeMask.z, edgeMask.w, distance(scenePosition, edgeMask.xy)) : 1.0;
        fragColor = vec4(min(softened.rgb + bloom, vec3(1.0)), alpha) * (qt_Opacity * edgeCoverage);
    }
}
