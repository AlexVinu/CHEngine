#pragma once

#include <Core.h>

#include <cstdint>
#include <cstring>

namespace CHEngine {
    enum class MaterialParameters
    {
        BASE_COLOR = 0,   // albedo / diffuse
        ROUGHNESS = 1,   // microsurface roughness
        METALLIC = 2,   // metalness
        NORMAL = 3,   // normal map

        AMBIENT_OCCLUSION = 4,   // AO map
        EMISSIVE = 5,   // emissive color / map

        OPACITY = 6,   // translucent alpha
        OPACITY_MASK = 7,   // alpha test / cutout

        HEIGHT = 8,   // parallax / height map
        DISPLACEMENT = 9,   // vertex displacement

        SPECULAR = 10,  // specular intensity (если поддерживаешь)
        CLEARCOAT = 11,  // второй specular слой
        CLEARCOAT_ROUGHNESS = 12,

        ANISOTROPY = 13,  // anisotropic BRDF
        SUBSURFACE = 14,  // SSS intensity
        SUBSURFACE_COLOR = 15,

        REFRACTION = 16,  // IOR / distortion

        DETAIL_COLOR = 17,  // detail albedo
        DETAIL_NORMAL = 18,  // detail normal

        WORLD_OFFSET = 19,  // vertex animation / WPO

        ORM = 20,  // packed texture (occlusion/roughness/metallic)

        COUNT = 21
    };

    using MaterialParamsStore = uint32_t;
// ─── Идентификаторы UBO-блоков ───────────────────────────────────────────
// Каждый enum = binding point (OpenGL) или buffer index (Metal).
enum class EUniformBlock : uint32_t
{
    Camera   = 0,  // данные камеры — раз в кадр
    Object   = 1,  // данные объекта — каждый draw call
    Lighting = 2,  // освещение — раз в кадр
    Material = 3,  // GL: layout(binding = 3); Metal: отдельный buffer во fragment (см. ShaderMTL)
};

// ─── std140-совместимые структуры ────────────────────────────────────────

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
    float Transform[16];     // mat4, offset 0
    float NormalMatrix[16];  // mat4, offset 64
    float Color[4];          // vec4, offset 128
    float Selected;          // float, offset 144
    float _pad0;             // offset 148
    float _pad1;             // offset 152
    float _pad2;             // offset 156
    // sizeof = 160 (std140 padding после Selected)

    UBOObject()
    {
        std::memset(this, 0, sizeof(*this));
        Transform[0]    = Transform[5]    = Transform[10]    = Transform[15]    = 1.0f;
        NormalMatrix[0] = NormalMatrix[5] = NormalMatrix[10] = NormalMatrix[15] = 1.0f;
        Color[0] = Color[1] = Color[2] = Color[3] = 1.0f;
    }
};

// ─── Binding 3: MaterialUBO ─────────────────────────────────────────────
// std140: scalars pack to 16 B; SpecularScale + padding rounds struct to 32 B.
struct alignas(16) UBOMaterial
{
    int32_t  UseTexture;     // offset 0
    int32_t  UseSpecularMap; // offset 4
    float    Shininess;      // offset 8
    MaterialParamsStore MaterialFlags;  // offset 12
    float    SpecularScale;  // offset 16
    float    _pad0;          // offset 20
    float    _pad1;          // offset 24
    float    _pad2;          // offset 28
    // sizeof = 32

    UBOMaterial()
    {
        std::memset(this, 0, sizeof(*this));
        Shininess      = 32.0f;
        SpecularScale  = 1.0f;
    }
};

static_assert(sizeof(UBOMaterial) == 32, "UBOMaterial must match GLSL/Metal std140 layout");

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
