#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoords;
layout(location = 3) in vec3 a_Color;

out vec3 v_Normal;
out vec3 v_FragPos;
out vec3 v_Color;
out vec2 v_TexCoords;

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
    vec4 worldPos = object.Transform * vec4(a_Position, 1.0);
    v_FragPos     = worldPos.xyz;
    v_Normal      = mat3(object.NormalMatrix) * a_Normal;
    v_Color       = a_Color;
    v_TexCoords   = a_TexCoords;
    gl_Position   = camera.ViewProjection * worldPos;
}
