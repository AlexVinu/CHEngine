#pragma once

#include <Core.h>
#include <CheStl/Vector.h>

#include "Render/Handles.h"
#include "Render/Core/RenderTypes.h"
#include "Render/Descriptors.h"

#include <string>
#include <glm/glm.hpp>

namespace CHEngine {

    // ─── Per-draw-call descriptor ─────────────────────────────────────────────

    struct DrawDesc {
        BufferHandle            VertexBuffer;
        BufferHandle            IndexBuffer;
        IndexFormat             IdxFormat     = IndexFormat::UInt32;
        uint32_t                IndexCount    = 0;
        uint32_t                FirstIndex    = 0;
        int32_t                 BaseVertex    = 0;
        uint32_t                InstanceCount = 1;
        Vector<UniformBinding>  Uniforms;
        Vector<TextureBinding>  Textures;
    };

    // ─── Per-pass descriptor ──────────────────────────────────────────────────

    struct PassDesc {
        std::string             Name;
        PipelineHandle          Pipeline;

        // Attachments
        Vector<TextureHandle>   ColorAttachments;
        TextureHandle           DepthAttachment;

        // Load / store ops
        ELoadOp                 ColorLoadOp   = ELoadOp::Clear;
        EStoreOp                ColorStoreOp  = EStoreOp::Store;
        glm::vec4               ClearColor    = { 0.0f, 0.0f, 0.0f, 1.0f };
        ELoadOp                 DepthLoadOp   = ELoadOp::Clear;
        float                   ClearDepth    = 1.0f;

        // Viewport
        uint32_t                ViewportWidth  = 0;
        uint32_t                ViewportHeight = 0;

        // Per-pass resource bindings
        Vector<UniformBinding>  Uniforms;
        Vector<TextureBinding>  Textures;

        // Dependency tracking for topo-sort
        Vector<TextureHandle>   Reads;
        Vector<TextureHandle>   Writes;

        // Draw calls
        Vector<DrawDesc>        Draws;

        // When true, backend ignores Draws and calls DrawFullscreenTriangle()
        // instead (used by TonemapPass and other fullscreen post-process passes).
        bool                    Fullscreen = false;
    };

} // namespace CHEngine
