#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoords;
layout(location = 3) in vec3 a_Color;

out vec3 v_Normal;
out vec3 v_FragPos;
out vec3 v_Color;
out vec2 v_TexCoords;

uniform mat4 u_ViewProjection;
uniform mat4 u_Transform;
uniform mat4 u_NormalMatrix;

void main()
{
    vec4 worldPos = u_Transform * vec4(a_Position, 1.0);
    v_FragPos     = worldPos.xyz;
    v_Normal      = mat3(u_NormalMatrix) * a_Normal;
    v_Color       = a_Color;
    v_TexCoords   = a_TexCoords;
    gl_Position   = u_ViewProjection * worldPos;
}
