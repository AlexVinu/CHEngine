#include <chepch.h>
#include "FrameGraphBackendOGL.h"

#include "RenderFactoryOGL.h"
#include "PipelineOGL.h"
#include "BufferOGL.h"
#include "TextureOGL.h"

#include <Log/Log.h>
#include <glad/glad.h>

namespace CHModules {

    FrameGraphBackendOGL::FrameGraphBackendOGL(RenderFactoryOGL& factory)
        : m_Factory(factory)
    {
    }

    FrameGraphBackendOGL::~FrameGraphBackendOGL()
    {
        // Delete all cached VAOs.
        for (auto& [k, vao] : m_VaoCache)
            if (vao) glDeleteVertexArrays(1, &vao);
        m_VaoCache.clear();

        // Delete all cached FBOs.
        for (auto& [k, fbo] : m_FboCache)
            if (fbo) glDeleteFramebuffers(1, &fbo);
        m_FboCache.clear();
    }

    // ─── FBO helper ──────────────────────────────────────────────────────────

    GLuint FrameGraphBackendOGL::GetOrCreateFBO(const CHEngine::PassDesc& pass)
    {
        // ── Get color and depth textures ───────────────────────────────────────
        CHEngine::TextureHandle colorTex = CHEngine::TextureHandle::Invalid();
        CHEngine::TextureHandle depthTex = CHEngine::TextureHandle::Invalid();

        // Get first color attachment
        if (!pass.ColorAttachments.empty())
            colorTex = pass.ColorAttachments[0];

        // Get depth attachment
        if (pass.DepthAttachment.IsValid())
            depthTex = pass.DepthAttachment;

        const bool hasColor = colorTex.IsValid();
        const bool hasDepth = depthTex.IsValid();
        const uint32_t colorIdx = hasColor ? colorTex.index      : ~0u;
        const uint32_t colorGen = hasColor ? colorTex.generation : 0u;
        const uint32_t depthIdx = hasDepth ? depthTex.index      : ~0u;
        const uint32_t depthGen = hasDepth ? depthTex.generation : 0u;

        // Default framebuffer (no attachments specified).
        if (colorIdx == ~0u && depthIdx == ~0u)
            return 0;

        FboKey key{ colorIdx, colorGen, depthIdx, depthGen };
        auto it = m_FboCache.find(key);
        if (it != m_FboCache.end())
            return it->second;

        GLuint fbo = 0;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        // Attach color texture(s).
        uint32_t colorAttachmentIndex = 0;
        for (uint32_t i = 0; i < static_cast<uint32_t>(pass.ColorAttachments.size()); ++i)
        {
            CHEngine::TextureHandle th = pass.ColorAttachments[i];
            if (!th.IsValid()) continue;

            TextureOGL* tex = m_Factory.Textures.Get(th);
            if (!tex) continue;

            glFramebufferTexture2D(GL_FRAMEBUFFER,
                                   GL_COLOR_ATTACHMENT0 + colorAttachmentIndex,
                                   GL_TEXTURE_2D,
                                   tex->GetRendererID(), 0);
            ++colorAttachmentIndex;
        }

        // Attach depth texture.
        if (pass.DepthAttachment.IsValid())
        {
            TextureOGL* depthTexObj = m_Factory.Textures.Get(pass.DepthAttachment);
            if (depthTexObj)
                glFramebufferTexture2D(GL_FRAMEBUFFER,
                                       GL_DEPTH_STENCIL_ATTACHMENT,
                                       GL_TEXTURE_2D,
                                       depthTexObj->GetRendererID(), 0);
        }

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
            CHE_CORE_ERROR("FrameGraphBackendOGL: FBO incomplete (status=0x{:X}) for pass '{}'",
                           status, pass.Name.c_str());

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        m_FboCache[key] = fbo;
        return fbo;
    }

    // ─── VAO helper ───────────────────────────────────────────────────────────

    GLuint FrameGraphBackendOGL::GetOrCreateVAO(const CHEngine::PassDesc& pass,
                                                 const CHEngine::DrawDesc& draw)
    {
        const bool hasPipe = pass.Pipeline.IsValid();
        const bool hasVB   = draw.VertexBuffer.IsValid();
        const bool hasIB   = draw.IndexBuffer.IsValid();
        VaoKey key{
            hasPipe ? pass.Pipeline.index      : ~0u,
            hasPipe ? pass.Pipeline.generation : 0u,
            hasVB   ? draw.VertexBuffer.index      : ~0u,
            hasVB   ? draw.VertexBuffer.generation : 0u,
            hasIB   ? draw.IndexBuffer.index       : ~0u,
            hasIB   ? draw.IndexBuffer.generation  : 0u,
        };
        auto it = m_VaoCache.find(key);
        if (it != m_VaoCache.end())
            return it->second;

        // Look up the pipeline to get the vertex layout.
        PipelineOGL* pipeline = m_Factory.Pipelines.Get(pass.Pipeline);
        BufferOGL*   vb       = m_Factory.Buffers.Get(draw.VertexBuffer);
        BufferOGL*   ib       = draw.IndexBuffer.IsValid() ? m_Factory.Buffers.Get(draw.IndexBuffer) : nullptr;

        if (!pipeline || !vb)
        {
            CHE_CORE_WARN("FrameGraphBackendOGL: cannot create VAO — missing pipeline or VB for pass '{}'",
                           pass.Name.c_str());
            m_VaoCache[key] = 0;
            return 0;
        }

        GLuint glVao = 0;
        glGenVertexArrays(1, &glVao);
        glBindVertexArray(glVao);

        // Bind VB and set attrib pointers.
        glBindBuffer(GL_ARRAY_BUFFER, vb->GetID());

        const CHEngine::VertexInputLayout& layout = pipeline->GetVertexLayout();
        uint32_t attribIdx = 0;
        for (const auto& attr : layout.Attributes)
        {
            const uint32_t slotIdx = (attr.Slot < static_cast<uint32_t>(layout.Strides.size()))
                                     ? attr.Slot : 0u;
            const uint32_t stride  = layout.Strides.empty() ? 0u : layout.Strides[slotIdx];

            glEnableVertexAttribArray(attribIdx);

            GLenum    glType = GL_FLOAT;
            GLint     comps  = 1;
            GLboolean norm   = GL_FALSE;

            using VF = CHEngine::VertexFormat;
            switch (attr.Format) {
            case VF::Float:        glType = GL_FLOAT;        comps = 1; break;
            case VF::Float2:       glType = GL_FLOAT;        comps = 2; break;
            case VF::Float3:       glType = GL_FLOAT;        comps = 3; break;
            case VF::Float4:       glType = GL_FLOAT;        comps = 4; break;
            case VF::Int:          glType = GL_INT;          comps = 1; break;
            case VF::Int2:         glType = GL_INT;          comps = 2; break;
            case VF::Int3:         glType = GL_INT;          comps = 3; break;
            case VF::Int4:         glType = GL_INT;          comps = 4; break;
            case VF::UInt:         glType = GL_UNSIGNED_INT; comps = 1; break;
            case VF::UInt2:        glType = GL_UNSIGNED_INT; comps = 2; break;
            case VF::UInt3:        glType = GL_UNSIGNED_INT; comps = 3; break;
            case VF::UInt4:        glType = GL_UNSIGNED_INT; comps = 4; break;
            case VF::UByte4:       glType = GL_UNSIGNED_BYTE; comps = 4; norm = GL_FALSE; break;
            case VF::UByte4_Norm:  glType = GL_UNSIGNED_BYTE; comps = 4; norm = GL_TRUE;  break;
            default: break;
            }

            if (glType == GL_INT || glType == GL_UNSIGNED_INT)
            {
                glVertexAttribIPointer(attribIdx, comps, glType,
                                       static_cast<GLsizei>(stride),
                                       reinterpret_cast<const void*>(static_cast<uintptr_t>(attr.Offset)));
            }
            else
            {
                glVertexAttribPointer(attribIdx, comps, glType, norm,
                                      static_cast<GLsizei>(stride),
                                      reinterpret_cast<const void*>(static_cast<uintptr_t>(attr.Offset)));
            }
            ++attribIdx;
        }

        // Bind IB inside the VAO.
        if (ib)
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib->GetID());

        glBindVertexArray(0);

        m_VaoCache[key] = glVao;
        return glVao;
    }

    // ─── Execute ─────────────────────────────────────────────────────────────

    static void CheckGLErrors(const char* where)
    {
        GLenum err;
        while ((err = glGetError()) != GL_NO_ERROR)
            CHE_CORE_ERROR("GL error 0x{:X} at {}", (unsigned)err, where);
    }

    void FrameGraphBackendOGL::Execute(const Vector<CHEngine::PassDesc>& passes)
    {
        for (const CHEngine::PassDesc& pass : passes)
        {
            // ── FBO setup ─────────────────────────────────────────────────────
            GLuint fbo = GetOrCreateFBO(pass);
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);

            // ── Viewport ──────────────────────────────────────────────────────
            if (pass.ViewportWidth > 0 && pass.ViewportHeight > 0)
                glViewport(0, 0,
                           static_cast<GLsizei>(pass.ViewportWidth),
                           static_cast<GLsizei>(pass.ViewportHeight));

            // ── Clear ─────────────────────────────────────────────────────────
            // glClear respects glColorMask/glDepthMask — reset them before clearing
            // so a previous pass's disabled write mask doesn't silently suppress the clear.
            GLbitfield clearMask = 0;
            if (pass.ColorLoadOp == CHEngine::ELoadOp::Clear)
            {
                glClearColor(pass.ClearColor.r, pass.ClearColor.g,
                             pass.ClearColor.b, pass.ClearColor.a);
                clearMask |= GL_COLOR_BUFFER_BIT;
            }
            if (pass.DepthLoadOp == CHEngine::ELoadOp::Clear)
            {
                glClearDepth(static_cast<GLdouble>(pass.ClearDepth));
                clearMask |= GL_DEPTH_BUFFER_BIT;
            }
            if (clearMask)
            {
                if (clearMask & GL_COLOR_BUFFER_BIT)
                    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                if (clearMask & GL_DEPTH_BUFFER_BIT)
                    glDepthMask(GL_TRUE);
                glClear(clearMask);
            }

            // ── Pipeline state ────────────────────────────────────────────────
            PipelineOGL* pipeline = m_Factory.Pipelines.Get(pass.Pipeline);
            if (!pipeline)
            {
                CHE_CORE_WARN("FrameGraphBackendOGL: pass '{}' has no valid pipeline, skipping",
                              pass.Name.c_str());
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                continue;
            }
            pipeline->Apply(m_Factory);

            // ── Per-pass uniform buffers ───────────────────────────────────────
            for (const CHEngine::UniformBinding& ub : pass.Uniforms)
            {
                BufferOGL* buf = m_Factory.Buffers.Get(ub.Buffer);
                if (!buf) continue;
                if (ub.Size > 0)
                    buf->BindRange(ub.Slot,
                                   static_cast<uint64_t>(ub.Offset),
                                   static_cast<uint64_t>(ub.Size));
                else
                    buf->BindBase(ub.Slot);
            }

            // ── Per-pass textures ─────────────────────────────────────────────
            for (const CHEngine::TextureBinding& tb : pass.Textures)
            {
                TextureOGL* tex = m_Factory.Textures.Get(tb.Texture);
                if (tex) tex->Bind(tb.Slot);
            }

            // ── Draw calls ────────────────────────────────────────────────────
            if (pass.Fullscreen)
            {
                m_Factory.Api.DrawFullscreenTriangle();
            }
            else
            {
                for (const CHEngine::DrawDesc& draw : pass.Draws)
                {
                    if (draw.IndexCount == 0) continue;

                    GLuint vao = GetOrCreateVAO(pass, draw);
                    if (!vao) continue;

                    glBindVertexArray(vao);

                    // Per-draw uniform buffers.
                    for (const CHEngine::UniformBinding& ub : draw.Uniforms)
                    {
                        BufferOGL* buf = m_Factory.Buffers.Get(ub.Buffer);
                        if (!buf) continue;
                        if (ub.Size > 0)
                            buf->BindRange(ub.Slot, ub.Offset, ub.Size);
                        else
                            buf->BindBase(ub.Slot);
                    }

                    // Per-draw textures.
                    for (const CHEngine::TextureBinding& tb : draw.Textures)
                    {
                        TextureOGL* tex = m_Factory.Textures.Get(tb.Texture);
                        if (tex) tex->Bind(tb.Slot);
                    }

                    GLenum glPrim    = ToGL(pipeline->GetPrimitive());
                    GLenum idxType   = ToGLIndexType(draw.IdxFormat);
                    const void* idxOffset = reinterpret_cast<const void*>(
                        static_cast<uintptr_t>(draw.FirstIndex) *
                        (draw.IdxFormat == CHEngine::IndexFormat::UInt16 ? 2u : 4u));

                    glDrawElementsInstancedBaseVertex(
                        glPrim,
                        static_cast<GLsizei>(draw.IndexCount),
                        idxType,
                        idxOffset,
                        static_cast<GLsizei>(draw.InstanceCount),
                        static_cast<GLint>(draw.BaseVertex));

                    CheckGLErrors(pass.Name.c_str());
                }
            }

            // ── Cleanup ───────────────────────────────────────────────────────
            glBindVertexArray(0);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
    }

} // namespace CHModules
