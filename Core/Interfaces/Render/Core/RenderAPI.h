#pragma once

// RenderAPI.h — canonical include for ERenderAPI, RendererInitInfo, RenderContextInfo.
// These types live in Core/src/RenderData.h (shared across engine + modules).
// This header re-exports them under the Render/Core/ path so Render interface headers
// can reference them without reaching into Core/src/.

#include "RenderData.h"   // ERenderAPI, RendererInitInfo (OpenGLInitData / VulkanInitData / MetalInitData union)

namespace CHEngine {

    /// Opaque per-frame render context for ImGui backends.
    /// Metal fills Device/CommandBuffer/RenderEncoder.
    /// OpenGL leaves all fields nullptr.
    struct RenderContextInfo
    {
        void* Device               = nullptr;
        void* CommandBuffer        = nullptr;
        void* RenderEncoder        = nullptr;
        void* RenderPassDescriptor = nullptr;
    };

    /// Legacy colour-format enum used by FramebufferOGL and RGResourceAllocator.
    /// Only RGBA8 and RGBA16F are required for the current render pipeline.
    enum class FramebufferColorFormat : uint8_t
    {
        RGBA8   = 0,
        RGBA16F = 1,
    };

} // namespace CHEngine
