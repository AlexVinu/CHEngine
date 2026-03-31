#version 330 core

layout(location = 0) out vec4 color;

in vec3 v_Color;

layout(std140) uniform ObjectUBO
{
    mat4  Transform;
    mat4  NormalMatrix;
    vec4  Color;
    float Selected;
    float _pad0;
    float _pad1;
    float _pad2;
} object;

void main()
{
    color = vec4(v_Color, 1.0) * object.Color;
}
