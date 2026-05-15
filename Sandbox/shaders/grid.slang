import common;

ConstantBuffer<CameraUBO> camera;

struct VSIn
{
    float2 NDC : POSITION;
};

struct VSOut
{
    float4 Position : SV_Position;
    float2 NDC      : TEXCOORD0;
};

[shader("vertex")]
VSOut vertMain(VSIn input)
{
    VSOut o;
    o.NDC      = input.NDC;
    o.Position = float4(input.NDC, 0.9999, 1.0);
    return o;
}

// fwidth(float2) not supported on Metal — use component-wise fwidth
float gridLine(float2 xz, float cellSize)
{
    float2 coord = xz / cellSize;
    // Metal: use float2(fwidth(x), fwidth(y)) instead of fwidth(float2)
    float2 d     = max(float2(fwidth(coord.x), fwidth(coord.y)), float2(0.001, 0.001));
    float2 f     = abs(frac(coord - 0.5) - 0.5) / d;
    return 1.0 - clamp(min(f.x, f.y), 0.0, 1.0);
}

[shader("fragment")]
float4 fragMain(VSOut input) : SV_Target
{
    // Unproject NDC pixel → world space
    float4 nearH = mul(camera.InvViewProj, float4(input.NDC, -1.0, 1.0));
    float4 farH  = mul(camera.InvViewProj, float4(input.NDC,  1.0, 1.0));
    float3 nearP = nearH.xyz / nearH.w;
    float3 farP  = farH.xyz  / farH.w;

    // Ray → Y=0 plane intersection
    float denom = farP.y - nearP.y;
    if (abs(denom) < 1e-6) return float4(0.0, 0.0, 0.0, 0.0);
    float t = -nearP.y / denom;
    if (t < 0.0) return float4(0.0, 0.0, 0.0, 0.0);

    float3 world = nearP + t * (farP - nearP);
    float2 xz    = world.xz;

    // Adaptive horizon fade
    float camH     = max(abs(camera.CameraPos.y), 1.0);
    float fadeNear = camH * 80.0;
    float fadeFar  = camH * 160.0;

    float dist = length(world.xz - camera.CameraPos.xz);
    float fade = 1.0 - smoothstep(fadeNear, fadeFar, dist);
    if (fade < 0.001) return float4(0.0, 0.0, 0.0, 0.0);

    // Three LOD levels — small grid visible much further (Blender-style)
    float d1fade = 1.0 - smoothstep(camH * 5.0,  camH * 14.0, dist);
    float d2fade = 1.0 - smoothstep(camH * 25.0, camH * 60.0, dist);

    float a1   = gridLine(xz,   1.0) * d1fade * 0.30;
    float a10  = gridLine(xz,  10.0) * d2fade * 0.55;
    float a100 = gridLine(xz, 100.0)          * 0.75;

    float alpha = max(a1, max(a10, a100)) * fade;

    // Grid color — dark enough to not glow after ACES tonemapping
    float3 color = float3(0.07, 0.07, 0.08);

    // Axis lines — exactly 1px, no glow.
    // Colors are pre-ACES-corrected so after tonemapping they appear as
    // Blender-style red (#C04040) and green (#40C040) — not near-white.
    float ax = max(fwidth(xz.y), 0.001);
    float az = max(fwidth(xz.x), 0.001);
    float xa = 1.0 - smoothstep(0.0, ax, abs(xz.y));
    float za = 1.0 - smoothstep(0.0, az, abs(xz.x));

    if (xa > za && xa > 0.01)
    {
        color = float3(0.30, 0.03, 0.03);  // X axis — red  (pre-ACES → ~#C04040)
        alpha = max(alpha, xa * fade);
    }
    else if (za > 0.01)
    {
        color = float3(0.03, 0.28, 0.04);  // Z axis — green (pre-ACES → ~#40C040)
        alpha = max(alpha, za * fade);
    }

    if (alpha < 0.002) return float4(0.0, 0.0, 0.0, 0.0);

    return float4(color, alpha);
}
