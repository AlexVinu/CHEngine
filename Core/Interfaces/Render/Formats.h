#pragma once

#include <cstdint>

namespace CHEngine {

    enum class EFormat : uint8_t
    {
        Unknown = 0,
        R8,
        RG8,
        RGB8,
        RGBA8,
        BGRA8,
        RGBA16F,
        RGBA32F,
        Depth24Stencil8,
        Depth32
    };

    enum class ELoadOp : uint8_t
    {
        Load,
        Clear,
        DontCare
    };

    enum class EStoreOp : uint8_t
    {
        Store,
        DontCare
    };

    enum class EPrimitiveTopology : uint8_t
    {
        Triangles,
        Lines
    };

    enum class ECullMode : uint8_t
    {
        None,
        Front,
        Back
    };

    struct EClearValue
    {
        float Color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        float Depth = 1.0f;
        uint32_t Stencil = 0;
    };

} // namespace CHEngine
