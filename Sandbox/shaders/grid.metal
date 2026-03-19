#include <metal_stdlib>
using namespace metal;

struct VertexIn {
    float2 ndc [[attribute(0)]];
};

struct VertexOut {
    float4 position [[position]];
    float2 ndc;
};

struct CameraUBO {
    float4x4 ViewProjection;
    float4x4 InvViewProj;
    float4   CameraPos;
};

vertex VertexOut vertexMain(VertexIn in [[stage_in]])
{
    VertexOut out;
    out.ndc      = in.ndc;
    // Z=0.9999, W=1.0 → after Metal remap: (0.9999+1)*0.5 = 0.99995 (still at far plane)
    out.position = float4(in.ndc, 0.99990, 1.0);
    out.position.z = (out.position.z + out.position.w) * 0.5;
    return out;
}

// ─── Grid line coverage ─────────────────────────────────────────────────────
float gridLine(float2 xz, float cellSize)
{
    float2 coord = xz / cellSize;
    float2 d     = max(fwidth(coord), float2(0.001));
    float2 f     = abs(fract(coord - 0.5) - 0.5) / d;
    return 1.0 - clamp(min(f.x, f.y), 0.0, 1.0);
}

fragment float4 fragmentMain(VertexOut in [[stage_in]],
                             constant CameraUBO& camera [[buffer(1)]])
{
    // Unproject NDC → world space
    float4 nearH = camera.InvViewProj * float4(in.ndc, -1.0, 1.0);
    float4 farH  = camera.InvViewProj * float4(in.ndc,  1.0, 1.0);
    float3 nearP = nearH.xyz / nearH.w;
    float3 farP  = farH.xyz  / farH.w;

    // Ray → Y=0 plane
    float denom = farP.y - nearP.y;
    if (abs(denom) < 1e-6) discard_fragment();
    float t = -nearP.y / denom;
    if (t < 0.0) discard_fragment();

    float3 world = nearP + t * (farP - nearP);
    float2 xz    = world.xz;

    // Adaptive horizon fade
    float camH    = max(abs(camera.CameraPos.y), 1.0);
    float fadeNear = camH * 40.0;
    float fadeFar  = camH * 80.0;

    float dist = length(world.xz - camera.CameraPos.xz);
    float fade = 1.0 - smoothstep(fadeNear, fadeFar, dist);
    if (fade < 0.001) discard_fragment();

    // Three LOD levels
    float d1fade = 1.0 - smoothstep(camH * 1.5, camH * 4.0,  dist);
    float d2fade = 1.0 - smoothstep(camH * 12.0, camH * 30.0, dist);

    float a1   = gridLine(xz,   1.0) * d1fade * 0.40;
    float a10  = gridLine(xz,  10.0) * d2fade * 0.70;
    float a100 = gridLine(xz, 100.0)          * 0.90;

    float alpha = max(a1, max(a10, a100)) * fade;

    // Grid base color
    float3 color = float3(0.24, 0.24, 0.24);

    // Axis lines
    float xa = 1.0 - clamp(abs(xz.y) / max(fwidth(xz.y) * 1.2, 0.001), 0.0, 1.0);
    float za = 1.0 - clamp(abs(xz.x) / max(fwidth(xz.x) * 1.2, 0.001), 0.0, 1.0);

    color = mix(color, float3(0.85, 0.20, 0.20), xa);  // X = red
    color = mix(color, float3(0.20, 0.40, 0.90), za);  // Z = blue
    alpha = max(alpha, max(xa, za) * fade);

    if (alpha < 0.002) discard_fragment();

    return float4(color, alpha);
}
