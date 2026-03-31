#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Color;

out vec3 v_Color;

layout(std140) uniform CameraUBO
{
    mat4 ViewProjection;
    mat4 InvViewProj;
    vec4 CameraPos;
} camera;

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
    v_Color     = a_Color;
    gl_Position = camera.ViewProjection * object.Transform * vec4(a_Position, 1.0);
}
