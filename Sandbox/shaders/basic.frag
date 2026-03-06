#version 330 core

layout(location = 0) out vec4 color;

in vec3 v_Position;

// Tint color set from CPU via uniform.
// Default (1,1,1,1) = no tint, shows position gradient.
uniform vec4 u_Color;

void main()
{
    vec4 posColor = vec4(v_Position * 0.5 + 0.5, 1.0);
    color = posColor * u_Color;
}
