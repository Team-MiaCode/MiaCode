#version 440
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texCoord;
layout(location = 0) out vec2 uv;
layout(std140, binding = 0) uniform buf { mat4 viewProjection; };
void main()
{
    uv = vec2(texCoord.x, 1.0 - texCoord.y);
    gl_Position = viewProjection * vec4(position, 1.0);
}
