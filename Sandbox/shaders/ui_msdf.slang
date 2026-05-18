// ui_msdf.slang
// In-game UI shader: solid color quads, textured quads, and MSDF/MTSDF text.
//
// UBO layout (matches UIRendererBackend):
//   slot 0 = UICameraUBO  (orthographic projection)
//   slot 1 = UIDrawUBO    (per-draw: color, params, mode)
// Texture slot 0 = atlas / image texture (used in mode 1 and 2)
//
// Vertex layout (stride 20 bytes):
//   float2 Position  (screen pixels, origin top-left)
//   float2 TexCoord  (normalized UV)
//   float  ExtraU    (corner radius or SDF params, packed)

struct UICameraUBO
{
    float4x4 ViewProj;
    float2   ScreenSize;
    float    _pad0;
    float    _pad1;
};

struct UIDrawUBO
{
    float4 ColorA;   // primary color (text color, solid fill, tint)
    float4 ColorB;   // secondary color (border color)
    // params.x = cornerRadius (pixels)   — mode 0/1
    // params.y = borderWidth  (pixels)   — mode 0
    // params.z = distanceRange           — mode 2
    // params.w = unused
    float4 Params;
    uint   Mode;     // 0 = solid, 1 = textured, 2 = MSDF text
    uint   _pad0;
    uint   _pad1;
    uint   _pad2;
};

ConstantBuffer<UICameraUBO> uiCamera : register(b0);
ConstantBuffer<UIDrawUBO>   uiDraw   : register(b1);

Texture2D    uiTexture : register(t0);
SamplerState uiSampler : register(s0);

struct VSIn
{
    float2 Position : POSITION;
    float2 TexCoord : TEXCOORD0;
    float2 QuadMin  : TEXCOORD1;  // quad top-left  in screen pixels
    float2 QuadMax  : TEXCOORD2;  // quad bot-right in screen pixels
};

struct VSOut
{
    float4 ClipPos  : SV_Position;
    float2 UV       : TEXCOORD0;
    float2 QuadMin  : TEXCOORD1;
    float2 QuadMax  : TEXCOORD2;
    float2 ScreenPos: TEXCOORD3;  // screen-space position (pixels)
};

[shader("vertex")]
VSOut vertMain(VSIn input)
{
    VSOut o;
    // ViewProj: maps canvas-pixel coords (0,0)..(W,H) to clip space.
    // Overlay: ortho 0..W / H..0. World canvas: cameraVP * model * pixelToLocal.
    o.ClipPos   = mul(uiCamera.ViewProj, float4(input.Position, 0.0, 1.0));
    o.UV        = input.TexCoord;
    o.QuadMin   = input.QuadMin;
    o.QuadMax   = input.QuadMax;
    o.ScreenPos = input.Position;
    return o;
}

// ── SDF rounded-rect alpha ────────────────────────────────────────────────────
// Returns signed distance to a rounded rectangle (negative = inside).
float RoundedRectSDF(float2 p, float2 halfSize, float radius)
{
    float2 q = abs(p) - halfSize + float2(radius, radius);
    return length(max(q, float2(0.0, 0.0))) + min(max(q.x, q.y), 0.0) - radius;
}

// ── MTSDF median ─────────────────────────────────────────────────────────────
float Median(float r, float g, float b)
{
    return max(min(r, g), min(max(r, g), b));
}

[shader("fragment")]
float4 fragMain(VSOut input) : SV_Target
{
    float4 outColor = float4(0.0, 0.0, 0.0, 0.0);

    float2 quadCenter  = (input.QuadMin + input.QuadMax) * 0.5;
    float2 quadHalfSz  = (input.QuadMax - input.QuadMin) * 0.5;
    float2 localPos    = input.ScreenPos - quadCenter;

    float cornerRadius = uiDraw.Params.x;
    float borderWidth  = uiDraw.Params.y;

    // ── Mode 0: solid color with optional rounded corners + border ────────────
    if (uiDraw.Mode == 0u)
    {
        float dist  = RoundedRectSDF(localPos, quadHalfSz, cornerRadius);
        // Antialiased edge using fwidth per component (Metal-safe)
        float fw    = max(float(fwidth(dist.x)), 0.0001);
        float alpha = 1.0 - smoothstep(-fw, fw, dist);

        float4 fillColor   = uiDraw.ColorA;
        float4 borderColor = uiDraw.ColorB;

        if (borderWidth > 0.0 && borderColor.a > 0.0)
        {
            float borderDist = dist + borderWidth;
            float bfw        = max(float(fwidth(borderDist.x)), 0.0001);
            float innerAlpha = 1.0 - smoothstep(-bfw, bfw, borderDist);
            // Blend: border outside fill
            float4 blended = lerp(borderColor, fillColor, innerAlpha);
            outColor = float4(blended.rgb, blended.a * alpha);
        }
        else
        {
            outColor = float4(fillColor.rgb, fillColor.a * alpha);
        }
    }
    // ── Mode 1: textured quad (UIImage) ──────────────────────────────────────
    else if (uiDraw.Mode == 1u)
    {
        float4 texSample = uiTexture.Sample(uiSampler, input.UV);
        outColor = texSample * uiDraw.ColorA;

        // Optional rounded clip
        if (cornerRadius > 0.0)
        {
            float dist = RoundedRectSDF(localPos, quadHalfSz, cornerRadius);
            float fw   = max(float(fwidth(dist.x)), 0.0001);
            float alpha = 1.0 - smoothstep(-fw, fw, dist);
            outColor.a *= alpha;
        }
    }
    // ── Mode 2: MTSDF text ────────────────────────────────────────────────────
    else
    {
        float4 mtsdf = uiTexture.Sample(uiSampler, input.UV);

        // Multi-channel median (R, G, B channels carry MSDF)
        float sd = Median(mtsdf.r, mtsdf.g, mtsdf.b);
        // True SDF in alpha channel for sharper strokes at small sizes
        float trueSdf = mtsdf.a;
        // Blend MSDF and true SDF based on proximity to edge
        float sdf = lerp(sd, trueSdf, clamp((trueSdf - 0.5) * 4.0 + 0.5, 0.0, 1.0));

        float distanceRange = uiDraw.Params.z;
        // screenPxRange: how many screen pixels the distance range spans
        // Computed from atlas size change vs screen size — approximate via fwidth
        float2 unitRange = float2(distanceRange, distanceRange) /
                           float2(float(fwidth(input.UV.x)), float(fwidth(input.UV.y)));
        float screenPxRange = max(0.5 * (unitRange.x + unitRange.y), 1.0);

        float screenSd = screenPxRange * (sdf - 0.5);
        float alpha    = clamp(screenSd + 0.5, 0.0, 1.0);

        outColor = float4(uiDraw.ColorA.rgb, uiDraw.ColorA.a * alpha);
    }

    return outColor;
}
