#version 330 core

// Full-screen quad in NDC space (-1..1).
// The fragment shader unprojects each pixel to world space and draws
// the grid analytically — no geometry needed, just 2 triangles.
layout(location = 0) in vec2 a_NDC;

out vec2 v_NDC;

void main()
{
    v_NDC       = a_NDC;
    // Push to far plane so the grid renders behind all opaque objects
    gl_Position = vec4(a_NDC, 0.9999, 1.0);
}
