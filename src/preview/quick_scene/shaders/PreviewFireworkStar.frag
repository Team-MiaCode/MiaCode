#version 440
layout(location = 0) in vec2 uv;
layout(location = 1) in vec4 tint;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float dx;
    float dy;
    float pass;
    vec4 edgeMask;
};
void main() {
    vec2 point = uv * 2.0 - 1.0;
    vec2 p = sqrt(point * point + vec2(0.018 * 0.018));
    const float exponent = 0.60;
    float shape = pow(p.x, exponent) + pow(p.y, exponent) - 1.0;
    vec2 gradient = exponent * pow(max(p, vec2(0.001)), vec2(exponent - 1.0));
    float distanceToEdge = shape / length(gradient);
    float pixelWidth = max(length(fwidth(point)), 0.0001);
    float alpha = (1.0 - smoothstep(-pixelWidth * 0.5, pixelWidth * 0.5, distanceToEdge)) * tint.a * qt_Opacity;
    fragColor = vec4(tint.rgb * alpha, alpha);
}
