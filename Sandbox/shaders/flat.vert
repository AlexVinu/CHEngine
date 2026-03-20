#version 330 core

layout(location = 0) in vec3 a_Position;

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
    int   UseTexture;
    int   UseSpecularMap;
    float Shininess;
} object;

void main()
{
    gl_Position = camera.ViewProjection * object.Transform * vec4(a_Position, 1.0);
}
