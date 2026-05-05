#pragma once

#include "Render/Descriptors.h"
#include <glad/glad.h>

namespace CHModules {

    class ShaderOGL;

    /// Aggregated GL pipeline state built from PipelineDesc.
    /// Apply() sets all relevant GL state for a draw call.
    class PipelineOGL {
    public:
        explicit PipelineOGL(const CHEngine::PipelineDesc& desc);

        /// Bind the shader program and apply all fixed-function state.
        /// factory is used to look up ShaderOGL from ShaderHandle.
        void Apply(class RenderFactoryOGL& factory) const;

        const CHEngine::VertexInputLayout& GetVertexLayout() const { return m_Layout; }
        CHEngine::PrimitiveType            GetPrimitive()    const { return m_Primitive; }
        CHEngine::ShaderHandle             GetShaderHandle() const { return m_ShaderHandle; }

        // Rebuild from an updated desc (used by shader hot-reload).
        void Rebuild(const CHEngine::PipelineDesc& desc);

        const CHEngine::PipelineDesc& GetDesc() const { return m_Desc; }

    private:
        CHEngine::PipelineDesc       m_Desc;
        CHEngine::ShaderHandle       m_ShaderHandle;
        CHEngine::VertexInputLayout  m_Layout;
        CHEngine::PrimitiveType      m_Primitive;
        CHEngine::DepthState         m_Depth;
        CHEngine::BlendState         m_Blend;
        CHEngine::RasterState        m_Raster;
    };

    // ─── GL enum helpers ──────────────────────────────────────────────────────
    GLenum ToGL(CHEngine::CompareOp op);
    GLenum ToGL(CHEngine::BlendFactor f);
    GLenum ToGL(CHEngine::BlendOp op);
    GLenum ToGL(CHEngine::PrimitiveType p);
    GLenum ToGLIndexType(CHEngine::IndexFormat fmt);

} // namespace CHModules
