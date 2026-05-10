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

        // ─── Per-draw uniforms and textures ────────────────────────
        Vector<UniformBinding>  Uniforms;
        Vector<TextureBinding>  Textures;
    };

    // ─── Per-pass descriptor ──────────────────────────────────────────────────

    struct PassDesc {
        std::string             Name;
        PipelineHandle          Pipeline;

        // ─── RENDER TARGET INFO ───────────────────────────────────
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

        // ─── UNIFORMS AND TEXTURES ────────────────────────────────
        Vector<UniformBinding>  Uniforms;
        Vector<TextureBinding>  Textures;

        // ─── DEPENDENCY TRACKING (for topological sort) ────────────
        Vector<TextureHandle>   Reads;   // Textures read by this pass
        Vector<TextureHandle>   Writes;  // Textures written by this pass

        // Draw calls
        Vector<DrawDesc>        Draws;

        // When true, backend ignores Draws and calls DrawFullscreenTriangle()
        // instead (used by TonemapPass and other fullscreen post-process passes).
        bool                    Fullscreen = false;
    };

} // namespace CHEngine
