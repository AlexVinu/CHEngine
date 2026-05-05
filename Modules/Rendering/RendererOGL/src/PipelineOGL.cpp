#include <chepch.h>
#include "PipelineOGL.h"
#include "RenderFactoryOGL.h"
#include "ShaderOGL.h"

#include <Log/Log.h>

namespace CHModules {

    // ─── Enum conversions ─────────────────────────────────────────────────────

    GLenum ToGL(CHEngine::CompareOp op)
    {
        using C = CHEngine::CompareOp;
        switch (op) {
        case C::Never:        return GL_NEVER;
        case C::Less:         return GL_LESS;
        case C::Equal:        return GL_EQUAL;
        case C::LessEqual:    return GL_LEQUAL;
        case C::Greater:      return GL_GREATER;
        case C::NotEqual:     return GL_NOTEQUAL;
        case C::GreaterEqual: return GL_GEQUAL;
        case C::Always:       return GL_ALWAYS;
        }
        return GL_LESS;
    }

    GLenum ToGL(CHEngine::BlendFactor f)
    {
        using B = CHEngine::BlendFactor;
        switch (f) {
        case B::Zero:                 return GL_ZERO;
        case B::One:                  return GL_ONE;
        case B::SrcColor:             return GL_SRC_COLOR;
        case B::OneMinusSrcColor:     return GL_ONE_MINUS_SRC_COLOR;
        case B::DstColor:             return GL_DST_COLOR;
        case B::OneMinusDstColor:     return GL_ONE_MINUS_DST_COLOR;
        case B::SrcAlpha:             return GL_SRC_ALPHA;
        case B::OneMinusSrcAlpha:     return GL_ONE_MINUS_SRC_ALPHA;
        case B::DstAlpha:             return GL_DST_ALPHA;
        case B::OneMinusDstAlpha:     return GL_ONE_MINUS_DST_ALPHA;
        case B::ConstantColor:        return GL_CONSTANT_COLOR;
        case B::OneMinusConstantColor:return GL_ONE_MINUS_CONSTANT_COLOR;
        case B::ConstantAlpha:        return GL_CONSTANT_ALPHA;
        case B::OneMinusConstantAlpha:return GL_ONE_MINUS_CONSTANT_ALPHA;
        case B::SrcAlphaSaturate:     return GL_SRC_ALPHA_SATURATE;
        }
        return GL_ONE;
    }

    GLenum ToGL(CHEngine::BlendOp op)
    {
        using O = CHEngine::BlendOp;
        switch (op) {
        case O::Add:             return GL_FUNC_ADD;
        case O::Subtract:        return GL_FUNC_SUBTRACT;
        case O::ReverseSubtract: return GL_FUNC_REVERSE_SUBTRACT;
        case O::Min:             return GL_MIN;
        case O::Max:             return GL_MAX;
        }
        return GL_FUNC_ADD;
    }

    GLenum ToGL(CHEngine::PrimitiveType p)
    {
        using P = CHEngine::PrimitiveType;
        switch (p) {
        case P::Points:        return GL_POINTS;
        case P::Lines:         return GL_LINES;
        case P::LineStrip:     return GL_LINE_STRIP;
        case P::LineLoop:      return GL_LINE_LOOP;
        case P::Triangles:     return GL_TRIANGLES;
        case P::TriangleStrip: return GL_TRIANGLE_STRIP;
        case P::TriangleFan:   return GL_TRIANGLE_FAN;
        }
        return GL_TRIANGLES;
    }

    GLenum ToGLIndexType(CHEngine::IndexFormat fmt)
    {
        return (fmt == CHEngine::IndexFormat::UInt16) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
    }

    // ─── PipelineOGL ─────────────────────────────────────────────────────────

    PipelineOGL::PipelineOGL(const CHEngine::PipelineDesc& desc)
    {
        Rebuild(desc);
    }

    void PipelineOGL::Rebuild(const CHEngine::PipelineDesc& desc)
    {
        m_Desc          = desc;
        m_ShaderHandle  = desc.Shader;
        m_Layout        = desc.VertexLayout;
        m_Primitive     = desc.Primitive;
        m_Depth         = desc.Depth;
        m_Blend         = desc.Blend;
        m_Raster        = desc.Raster;
    }

    void PipelineOGL::Apply(RenderFactoryOGL& factory) const
    {
        // Bind shader program.
        if (ShaderOGL* shader = factory.Shaders.Get(m_ShaderHandle))
            shader->Bind();
        else
            CHE_CORE_WARN("PipelineOGL::Apply: invalid shader handle");

        // Depth state.
        if (m_Depth.Test)
        {
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(ToGL(m_Depth.Compare));
        }
        else
        {
            glDisable(GL_DEPTH_TEST);
        }
        glDepthMask(m_Depth.Write ? GL_TRUE : GL_FALSE);

        // Blend state.
        if (m_Blend.Enable)
        {
            glEnable(GL_BLEND);
            glBlendFuncSeparate(ToGL(m_Blend.SrcColor), ToGL(m_Blend.DstColor),
                                ToGL(m_Blend.SrcAlpha), ToGL(m_Blend.DstAlpha));
            glBlendEquationSeparate(ToGL(m_Blend.ColorOp), ToGL(m_Blend.AlphaOp));
        }
        else
        {
            glDisable(GL_BLEND);
        }

        // Rasterizer state.
        if (m_Raster.Cull == CHEngine::CullMode::None)
        {
            glDisable(GL_CULL_FACE);
        }
        else
        {
            glEnable(GL_CULL_FACE);
            glCullFace(m_Raster.Cull == CHEngine::CullMode::Front ? GL_FRONT : GL_BACK);
        }

        glFrontFace(m_Raster.Face == CHEngine::FrontFace::CCW ? GL_CCW : GL_CW);
        glPolygonMode(GL_FRONT_AND_BACK, m_Raster.Wireframe ? GL_LINE : GL_FILL);
    }

} // namespace CHModules
