#version 440
layout(location = 0) in vec2 uv;
layout(location = 1) in vec4 tint;
layout(location = 2) in vec2 scenePosition;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float animationTime;
    float exposure;
    float brightness;
    vec4 patternRect;
    vec4 materialAndEdge;
    vec4 clipBounds;
};
layout(binding = 1) uniform sampler2D sourceTexture;
void main()
{
    vec4 color = texture(sourceTexture, uv);
    float radius = distance(scenePosition, materialAndEdge.yz);
    float pixelWidth = max(fwidth(radius), 0.5);
    float coverage = 1.0 - smoothstep(clipBounds.x - pixelWidth, clipBounds.x, radius);
    if (materialAndEdge.w > 0.0) {
        coverage *= 1.0 - smoothstep(materialAndEdge.w, clipBounds.x, radius);
    }
    // The upstream SDR compositor clamps straight colour before desktop composition.
    fragColor = vec4(min(color.rgb, vec3(color.a)) * brightness, color.a)
        * (coverage * qt_Opacity);
}
