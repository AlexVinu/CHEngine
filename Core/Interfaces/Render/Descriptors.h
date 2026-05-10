#pragma once

#include <Core.h>
#include <CheStl/Vector.h>

#include "Render/Handles.h"
#include "Render/Core/RenderTypes.h"
#include "Render/Pipeline/VertexLayout.h"

namespace CHEngine {

    // ─── Comparison / rasterizer / blend enums ───────────────────────────────

    enum class CompareOp : uint8_t {
        Never, Less, Equal, LessEqual, Greater, NotEqual, GreaterEqual, Always
    };

    enum class CullMode : uint8_t { None, Front, Back };
    enum class FrontFace : uint8_t { CCW, CW };

    enum class BlendFactor : uint8_t {
        Zero, One,
        SrcColor, OneMinusSrcColor,
        DstColor, OneMinusDstColor,
        SrcAlpha, OneMinusSrcAlpha,
        DstAlpha, OneMinusDstAlpha,
        ConstantColor, OneMinusConstantColor,
        ConstantAlpha, OneMinusConstantAlpha,
        SrcAlphaSaturate
    };

    enum class BlendOp : uint8_t { Add, Subtract, ReverseSubtract, Min, Max };

    enum class ELoadOp  : uint8_t { Load, Clear, DontCare };
    enum class EStoreOp : uint8_t { Store, DontCare };

    // ─── Pipeline state structs ───────────────────────────────────────────────

    struct DepthState {
        bool      Test    = true;
        bool      Write   = true;
        CompareOp Compare = CompareOp::Less;
    };

    struct BlendState {
        bool        Enable  = false;
        BlendFactor SrcColor = BlendFactor::One;
        BlendFactor DstColor = BlendFactor::Zero;
        BlendOp     ColorOp  = BlendOp::Add;
        BlendFactor SrcAlpha = BlendFactor::One;
        BlendFactor DstAlpha = BlendFactor::Zero;
        BlendOp     AlphaOp  = BlendOp::Add;
    };

    struct RasterState {
        CullMode  Cull      = CullMode::Back;
        FrontFace Face      = FrontFace::CCW;
        bool      Wireframe = false;
    };

    struct PipelineDesc {
        ShaderHandle       Shader;
        VertexInputLayout  VertexLayout;
        PrimitiveType      Primitive     = PrimitiveType::Triangles;
        DepthState         Depth;
        BlendState         Blend;
        RasterState        Raster;
        Vector<TextureFormat> ColorFormats;
        TextureFormat      DepthFormat   = TextureFormat::D24_UNORM_S8_UINT;
    };

    // ─── Binding descriptors ──────────────────────────────────────────────────

    struct UniformBinding {
        BufferHandle Buffer;
        uint32_t     Slot   = 0;
        uint64_t     Offset = 0;
        uint64_t     Size   = 0; // 0 = whole buffer
    };

    struct TextureBinding {
        TextureHandle Texture;
        uint32_t      Slot = 0;
    };

} // namespace CHEngine
