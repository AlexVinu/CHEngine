#version 330 core

layout(location = 0) out vec4 color;

in vec3 v_Color;

layout(std140) uniform ObjectUBO
{
    mat4  Transform;
    mat4  NormalMatrix;
    vec4  Color;
    float Selected;
    int   UseTexture;
    int   UseSpecularMap;
    float Shininess;
} object;

void main()
{
    color = vec4(v_Color, 1.0) * object.Color;
}
