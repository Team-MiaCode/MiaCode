#version 440
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in vec4 color;
layout(location = 0) out vec2 uv;
layout(location = 1) out vec4 tint;
layout(location = 2) out vec2 scenePosition;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float dx;
    float dy;
    float pass;
    vec4 edgeMask;
};
void main() {
    uv = texCoord;
    scenePosition = position;
    tint = color;
    gl_Position = qt_Matrix * vec4(position, 0.0, 1.0);
}
