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
layout(binding = 2) uniform sampler2D colorTexture;
void main()
{
    vec4 tex = texture(sourceTexture, uv);
    vec3 rgb = tex.rgb / max(tex.a, 0.00001);
    if (materialAndEdge.x == 3.0) {
        vec4 second = texture(colorTexture, uv);
        rgb *= second.rgb / max(second.a, 0.00001);
    }
    rgb *= exp2(exposure) * tint.rgb;
    float alpha = tex.a * tint.a;
    float coverage = clamp(max(rgb.r, max(rgb.g, rgb.b)) * alpha, 0.0, 1.0);
    fragColor = vec4(rgb * alpha, coverage) * qt_Opacity;
}
