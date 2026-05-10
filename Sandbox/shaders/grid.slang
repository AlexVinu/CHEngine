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
    float fadeNear = camH * 40.0;
    float fadeFar  = camH * 80.0;

    float dist = length(world.xz - camera.CameraPos.xz);
    float fade = 1.0 - smoothstep(fadeNear, fadeFar, dist);
    if (fade < 0.001) return float4(0.0, 0.0, 0.0, 0.0);

    // Three LOD levels
    float d1fade = 1.0 - smoothstep(camH * 1.5,  camH * 4.0,  dist);
    float d2fade = 1.0 - smoothstep(camH * 12.0, camH * 30.0, dist);

    float a1   = gridLine(xz,   1.0) * d1fade * 0.40;
    float a10  = gridLine(xz,  10.0) * d2fade * 0.70;
    float a100 = gridLine(xz, 100.0)          * 0.90;

    float alpha = max(a1, max(a10, a100)) * fade;

    float3 color = float3(0.24, 0.24, 0.24);

    // Axis lines — Metal: scalar fwidth is supported
    float dxz_y  = max(fwidth(xz.y) * 1.2, 0.001);
    float dxz_x  = max(fwidth(xz.x) * 1.2, 0.001);
    float xa = 1.0 - clamp(abs(xz.y) / dxz_y, 0.0, 1.0);
    float za = 1.0 - clamp(abs(xz.x) / dxz_x, 0.0, 1.0);

    color = lerp(color, float3(0.85, 0.20, 0.20), xa);
    color = lerp(color, float3(0.20, 0.40, 0.90), za);
    alpha = max(alpha, max(xa, za) * fade);

    if (alpha < 0.002) return float4(0.0, 0.0, 0.0, 0.0);

    return float4(color, alpha);
}
