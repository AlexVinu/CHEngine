#version 330 core

in  vec2 v_NDC;
out vec4 FragColor;

uniform mat4 u_InvViewProj;   // inverse(projection * view)
uniform vec3 u_CameraPos;

// ─────────────────────────────────────────────────────────────────────────────
//  Grid line coverage for a given cell size.
//  Screen-space derivatives (fwidth) give pixel-perfect AA at any zoom.
// ─────────────────────────────────────────────────────────────────────────────
float gridLine(vec2 xz, float cellSize)
{
    vec2 coord = xz / cellSize;
    vec2 d     = max(fwidth(coord), vec2(0.001));
    vec2 f     = abs(fract(coord - 0.5) - 0.5) / d;
    return 1.0 - clamp(min(f.x, f.y), 0.0, 1.0);
}

void main()
{
    // ── Unproject NDC pixel → world space ────────────────────────────────────
    vec4 nearH = u_InvViewProj * vec4(v_NDC, -1.0, 1.0);
    vec4 farH  = u_InvViewProj * vec4(v_NDC,  1.0, 1.0);
    vec3 near  = nearH.xyz / nearH.w;
    vec3 far   = farH.xyz  / farH.w;

    // ── Ray → Y=0 plane intersection ─────────────────────────────────────────
    float denom = far.y - near.y;
    if (abs(denom) < 1e-6) discard;    // ray parallel to ground
    float t = -near.y / denom;
    if (t < 0.0) discard;              // intersection behind camera

    vec3 world = near + t * (far - near);
    vec2 xz    = world.xz;

    // ── Adaptive horizon fade ─────────────────────────────────────────────────
    // Scales with camera height so the grid always fades at the visible horizon.
    float camH    = max(abs(u_CameraPos.y), 1.0);
    float fadeNear = camH * 40.0;
    float fadeFar  = camH * 80.0;

    float dist = length(world.xz - u_CameraPos.xz);
    float fade = 1.0 - smoothstep(fadeNear, fadeFar, dist);
    if (fade < 0.001) discard;

    // ── Three LOD levels (Blender-style) ─────────────────────────────────────
    //   Level 1 :   1-unit cells  — fine grid,  fades close when zoomed out
    //   Level 2 :  10-unit cells  — medium grid, fades at mid distance
    //   Level 3 : 100-unit cells  — major grid,  always visible until horizon
    float d1fade = 1.0 - smoothstep(camH * 1.5, camH * 4.0,  dist);
    float d2fade = 1.0 - smoothstep(camH * 12.0, camH * 30.0, dist);

    float a1   = gridLine(xz,   1.0) * d1fade * 0.40;
    float a10  = gridLine(xz,  10.0) * d2fade * 0.70;
    float a100 = gridLine(xz, 100.0)          * 0.90;

    float alpha = max(a1, max(a10, a100)) * fade;

    // ── Grid base colour ──────────────────────────────────────────────────────
    vec3 color = vec3(0.24, 0.24, 0.24);

    // ── Axis lines — 1px wide, screen-space derived ───────────────────────────
    // X axis (z≈0) → red    Z axis (x≈0) → blue
    float xa = 1.0 - clamp(abs(xz.y) / max(fwidth(xz.y) * 1.2, 0.001), 0.0, 1.0);
    float za = 1.0 - clamp(abs(xz.x) / max(fwidth(xz.x) * 1.2, 0.001), 0.0, 1.0);

    color = mix(color, vec3(0.85, 0.20, 0.20), xa);   // X = red
    color = mix(color, vec3(0.20, 0.40, 0.90), za);   // Z = blue
    alpha  = max(alpha, max(xa, za) * fade);

    if (alpha < 0.002) discard;

    FragColor = vec4(color, alpha);
}
