#pragma once

#include "Memory/Handle.h"

namespace CHEngine {

    // Tag types for typed handles. Distinct types prevent accidental
    // mixing (e.g. passing a TextureHandle where a BufferHandle is expected).
    //
    // Only API-agnostic concepts live here. VAO (OGL-only), Framebuffer (OGL-only),
    // and RenderPass (VK-only) are backend-internal and never appear in Core/Interfaces.
    // Vertex layout is part of PipelineDesc; render targets are declared in PassDesc
    // via TextureHandle attachments.
    struct BufferTag {};
    struct TextureTag {};
    struct ShaderTag {};
    struct PipelineTag {};

    using BufferHandle   = Handle<BufferTag>;
    using TextureHandle  = Handle<TextureTag>;
    using ShaderHandle   = Handle<ShaderTag>;
    using PipelineHandle = Handle<PipelineTag>;

}
