#version 330 core

layout(location = 0) out vec4 color;

in vec3 v_Position;

void main()
{
    // Vertical blue-to-cyan gradient
    float t = clamp(v_Position.y * 0.5 + 0.5, 0.0, 1.0);
    color = vec4(0.0, t, 1.0, 1.0);
}
