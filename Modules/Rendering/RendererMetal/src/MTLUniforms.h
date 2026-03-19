#pragma once

#include <cstdint>
#include <cstring>

// ─── Uniform structs для Metal шейдеров ──────────────────────────────────
// Раскладка ДОЛЖНА точно совпадать с MSL struct'ами в .metal файлах.
// Используются float4 вместо float3 для гарантии 16-byte alignment.

namespace CHModules {

    struct alignas(16) MTLVertexUniforms
    {
        float ViewProjection[16]; // float4x4, offset 0
        float Transform[16];     // float4x4, offset 64
        float NormalMatrix[16];  // float4x4, offset 128
        float InvViewProj[16];   // float4x4, offset 192

        MTLVertexUniforms()
        {
            // Identity matrices
            std::memset(this, 0, sizeof(*this));
            ViewProjection[0] = ViewProjection[5] = ViewProjection[10] = ViewProjection[15] = 1.0f;
            Transform[0] = Transform[5] = Transform[10] = Transform[15] = 1.0f;
            NormalMatrix[0] = NormalMatrix[5] = NormalMatrix[10] = NormalMatrix[15] = 1.0f;
            InvViewProj[0] = InvViewProj[5] = InvViewProj[10] = InvViewProj[15] = 1.0f;
        }
    };
    // sizeof = 256

    struct alignas(16) MTLLightData
    {
        int32_t type      = 0;    // offset 0
        int32_t _pad0     = 0;    // offset 4
        int32_t _pad1     = 0;    // offset 8
        int32_t _pad2     = 0;    // offset 12
        float   pos[4]    = {};   // float4, offset 16 (.xyz used)
        float   dir[4]    = {};   // float4, offset 32 (.xyz used)
        float   colorIntensity[4] = {}; // float4, offset 48 (.rgb = color, .a = intensity)
        float   range     = 0;    // offset 64
        float   innerCone = 0;    // offset 68
        float   outerCone = 0;    // offset 72
        float   _pad3     = 0;    // offset 76
    };
    // sizeof = 80

    struct alignas(16) MTLFragmentUniforms
    {
        float   Color[4]       = {1,1,1,1};   // float4, offset 0
        float   CameraPos[4]   = {};           // float4, offset 16 (.xyz used)
        float   AmbientColor[4]= {0.15f,0.15f,0.2f,0}; // float4, offset 32 (.xyz used)
        int32_t NumLights      = 0;            // offset 48
        int32_t UseTexture     = 0;
        int32_t UseSpecularMap = 0;
        float   Shininess      = 32.0f;        // offset 60
        float   Selected       = 0;            // offset 64
        int32_t _pad0          = 0;
        int32_t _pad1          = 0;
        int32_t _pad2          = 0;            // offset 80
        MTLLightData Lights[8] = {};           // offset 80, 80*8 = 640
    };
    // sizeof = 720

} // namespace CHModules
