#pragma once

#include <cstdint>
#include <cstring>

namespace CHEngine {

// ─── Идентификаторы UBO-блоков ───────────────────────────────────────────
// Каждый enum = binding point (OpenGL) или buffer index (Metal).
enum class EUniformBlock : uint32_t
{
    Camera   = 0,  // данные камеры — раз в кадр
    Object   = 1,  // данные объекта — каждый draw call
    Lighting = 2,  // освещение — раз в кадр
};

// ─── std140-совместимые структуры ────────────────────────────────────────
// Раскладка идентична для OpenGL UBO (std140) и Metal buffer.
// Все vec3 заменены на float[4] (vec4) для гарантии 16-byte alignment.

// ─── Binding 0: CameraUBO ───────────────────────────────────────────────
// Обновляется один раз в кадр. Используется всеми шейдерами.
struct alignas(16) UBOCamera
{
    float ViewProjection[16]; // mat4, offset 0
    float InvViewProj[16];   // mat4, offset 64
    float CameraPos[4];      // vec4, offset 128 (.xyz)
    // sizeof = 144

    UBOCamera()
    {
        std::memset(this, 0, sizeof(*this));
        ViewProjection[0] = ViewProjection[5] = ViewProjection[10] = ViewProjection[15] = 1.0f;
        InvViewProj[0]    = InvViewProj[5]    = InvViewProj[10]    = InvViewProj[15]    = 1.0f;
    }
};

// ─── Binding 1: ObjectUBO ───────────────────────────────────────────────
// Обновляется на каждый draw call (per-object данные).
struct alignas(16) UBOObject
{
    float   Transform[16];     // mat4, offset 0
    float   NormalMatrix[16];  // mat4, offset 64
    float   Color[4];          // vec4, offset 128
    float   Selected;          // float, offset 144
    int32_t UseTexture;        // int,   offset 148
    int32_t UseSpecularMap;    // int,   offset 152
    float   Shininess;         // float, offset 156
    // sizeof = 160

    UBOObject()
    {
        std::memset(this, 0, sizeof(*this));
        Transform[0]    = Transform[5]    = Transform[10]    = Transform[15]    = 1.0f;
        NormalMatrix[0] = NormalMatrix[5] = NormalMatrix[10] = NormalMatrix[15] = 1.0f;
        Color[0] = Color[1] = Color[2] = Color[3] = 1.0f;
        Shininess = 32.0f;
    }
};

// ─── Данные одного источника света (внутри LightingUBO) ─────────────────
struct alignas(16) UBOLightData
{
    int32_t Type;              // offset 0
    int32_t _pad0;             // offset 4
    int32_t _pad1;             // offset 8
    int32_t _pad2;             // offset 12
    float   Position[4];       // vec4, offset 16 (.xyz)
    float   Direction[4];      // vec4, offset 32 (.xyz)
    float   ColorIntensity[4]; // vec4, offset 48 (.rgb=color, .a=intensity)
    float   Range;             // offset 64
    float   InnerCone;         // offset 68
    float   OuterCone;         // offset 72
    float   _pad3;             // offset 76
    // sizeof = 80 (кратно 16)

    UBOLightData() { std::memset(this, 0, sizeof(*this)); }
};

// ─── Binding 2: LightingUBO ────────────────────────────────────────────
// Обновляется один раз в кадр. Содержит ambient + массив источников.
static constexpr int MaxUBOLights = 8;

struct alignas(16) UBOLighting
{
    float   AmbientColor[4];              // vec4, offset 0 (.xyz)
    int32_t NumLights;                    // offset 16
    int32_t _pad0;                        // offset 20
    int32_t _pad1;                        // offset 24
    int32_t _pad2;                        // offset 28
    UBOLightData Lights[MaxUBOLights];    // offset 32, 80*8=640
    // sizeof = 672

    UBOLighting()
    {
        std::memset(this, 0, sizeof(*this));
        AmbientColor[0] = 0.15f;
        AmbientColor[1] = 0.15f;
        AmbientColor[2] = 0.2f;
    }
};

} // namespace CHEngine
