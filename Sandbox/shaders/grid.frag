#version 330 core

in  vec2 v_NDC;
out vec4 FragColor;

uniform mat4 u_InvViewProj;  // inverse(projection * view)
uniform vec3 u_CameraPos;

// Returns 0..1 coverage of grid lines for a given world cell size.
// Uses screen-space derivatives for anti-aliasing (no aliasing at any zoom).
float gridLine(vec2 xz, float cellSize)
{
    vec2 coord = xz / cellSize;
    vec2 d     = max(fwidth(coord), vec2(0.001));
    vec2 f     = abs(fract(coord - 0.5) - 0.5) / d;
    return 1.0 - clamp(min(f.x, f.y), 0.0, 1.0);
}

void main()
{
    // ── Unproject NDC pixel to world-space ────────────────────────────────
    vec4 nearH = u_InvViewProj * vec4(v_NDC, -1.0, 1.0);
    vec4 farH  = u_InvViewProj * vec4(v_NDC,  1.0, 1.0);
    vec3 near  = nearH.xyz / nearH.w;
    vec3 far   = farH.xyz  / farH.w;

    // ── Intersect ray with the Y = 0 (ground) plane ───────────────────────
    float denom = far.y - near.y;
    if (abs(denom) < 1e-6) discard;  // ray parallel to plane
    float t = -near.y / denom;
    if (t < 0.0) discard;            // intersection behind camera

    vec3 world = near + t * (far - near);
    vec2 xz    = world.xz;

    // ── Distance fade (grid disappears at horizon) ────────────────────────
    float dist = length(world - u_CameraPos);
    float fade = 1.0 - smoothstep(60.0, 120.0, dist);
    if (fade < 0.001) discard;

    // ── Two LOD levels ────────────────────────────────────────────────────
    // Fine cells (1 unit) fade out when zoomed out
    float fineFade = 1.0 - smoothstep(8.0, 24.0, dist);
    float a1  = gridLine(xz,  1.0) * fineFade * 0.50;
    float a10 = gridLine(xz, 10.0) * 0.90;
    float alpha = max(a1, a10) * fade;

    // ── Axis colouring ────────────────────────────────────────────────────
    vec3 color = vec3(0.27, 0.27, 0.27);

    // X axis  (z ≈ 0) — red
    float xa = 1.0 - clamp(abs(xz.y) / max(fwidth(xz.y), 0.001), 0.0, 1.0);
    // Z axis  (x ≈ 0) — blue
    float za = 1.0 - clamp(abs(xz.x) / max(fwidth(xz.x), 0.001), 0.0, 1.0);

    color = mix(color, vec3(0.80, 0.18, 0.18), xa);
    color = mix(color, vec3(0.18, 0.35, 0.85), za);
    alpha = max(alpha, max(xa, za) * fade);

    if (alpha < 0.002) discard;

    FragColor = vec4(color, alpha);
}
