#version 440

layout(location = 0) in vec2 aPosition;
layout(location = 0) out vec2 vPosition;

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

void main()
{
    vPosition = aPosition;
    gl_Position = qt_Matrix * vec4(aPosition, 0.0, 1.0);
}
