// Fullscreen tonemap pass.
// Vertex input is a single float2 position in clip space. The pipeline supplies
// a covering triangle (DrawFullscreenTriangle), so this shader stays portable
// across OpenGL / Vulkan without relying on SV_VertexID (which Slang emits as
// gl_VertexIndex — unavailable in plain GLSL without GL_KHR_vulkan_glsl).

Sampler2D u_HDRTexture;

struct VSIn
{
    [[vk::location(0)]] float2 Position : POSITION;
};

struct VSOut
{
    float4 Position : SV_Position;
    float2 UV       : TEXCOORD0;
};

[shader("vertex")]
VSOut vertMain(VSIn input)
{
    VSOut o;
    o.Position = float4(input.Position, 0.0, 1.0);
    // No Y flip: HDR FBO already stores scene with OpenGL convention
    // (V=1 at scene-top). The downstream ImGui::Image does the Y flip
    // that the OpenGL FBO->screen path expects.
    o.UV = input.Position * 0.5 + 0.5;
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
    ldrColor = pow(max(ldrColor, float3(0.0, 0.0, 0.0)), float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));

    return float4(ldrColor, 1.0);
}
