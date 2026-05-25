// Fullscreen tonemap pass.
// Uses SV_VertexID to generate a full-screen triangle without a vertex buffer.
// Metal supports this natively. OpenGL uses gl_VertexID (GLSL 1.30+).

struct TonemapParamsUBO
{
    float GammaCorrect; // 1.0 = apply manual pow(1/2.2),  0.0 = skip (GPU SRGB swapchain does it)
    float _pad0, _pad1, _pad2;
};

// Params UBO at register(b0) → Vulkan binding 0.
// Texture at explicit binding 1 to avoid flat-namespace conflict with b0 in SPIRV.
ConstantBuffer<TonemapParamsUBO> params;
[[vk::binding(1, 0)]]
Sampler2D u_HDRTexture;

struct VSOut
{
    float4 Position : SV_Position;
    float2 UV       : TEXCOORD0;
};

[shader("vertex")]
VSOut vertMain(uint vertexID : SV_VertexID)
{
    // Generate a triangle that covers the entire clip space:
    //  vertex 0: (-1, -1)  UV (0,  0)
    //  vertex 1: ( 3, -1)  UV (2,  0)
    //  vertex 2: (-1,  3)  UV (0,  2)
    float2 positions[3] = { float2(-1.0, -1.0), float2(3.0, -1.0), float2(-1.0, 3.0) };
    float2 pos = positions[vertexID];
    VSOut o;
    o.Position = float4(pos, 0.0, 1.0);
    // Correct UV mapping for Metal:
    // pos.y=+1 (screen top)    → UV.y=0 (sample HDR top row = sky)
    // pos.y=-1 (screen bottom) → UV.y=1 (sample HDR bottom row = ground)
    o.UV = float2(pos.x * 0.5 + 0.5, 0.5 - pos.y * 0.5);
    return o;
}

// Simple ACES approximation (Narkowicz 2015).
float3 ACESApprox(float3 x)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

[shader("fragment")]
float4 fragMain(VSOut input) : SV_Target
{
    float3 hdrColor = u_HDRTexture.Sample(input.UV).rgb;

    // Apply ACES approximation for a filmic look.
    float3 ldrColor = ACESApprox(hdrColor);

    // Gamma correction: linear -> sRGB (approximate, gamma = 2.2).
    // Skipped when the target API (Vulkan SRGB swapchain) already applies gamma automatically,
    // because applying it twice would produce a washed-out, overexposed image.
    if (params.GammaCorrect > 0.5)
        ldrColor = pow(max(ldrColor, float3(0.0, 0.0, 0.0)), float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));

    return float4(ldrColor, 1.0);
}
