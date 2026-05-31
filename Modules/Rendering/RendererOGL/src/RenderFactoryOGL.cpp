#include "RenderFactoryOGL.h"
#include "FrameGraphBackendOGL.h"

#include <glad/glad.h>
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace CHModules {

    // ============================================================
    // Buffer
    // ============================================================

    CHEngine::BufferHandle RenderFactoryOGL::CreateBuffer(
        uint64_t size,
        CHEngine::BufferUsage usage,
        CHEngine::MemoryType memory,
        std::span<const std::byte> initialData,
        const String& debugName)
    {
        auto* buf = new BufferOGL(size, usage, memory, initialData, debugName);
        return Buffers.Add(buf);
    }

    void RenderFactoryOGL::Delete(CHEngine::BufferHandle handle)
    {
        Buffers.Remove(handle);
    }

    // ============================================================
    // Shader
    // ============================================================

    CHEngine::ShaderHandle RenderFactoryOGL::CreateShader(
        const String& slangSource,
        const String& vertEntry,
        const String& fragEntry,
        const String& sourcePath)
    {
        auto* shader = new ShaderOGL(slangSource, vertEntry, fragEntry, sourcePath, Slang);
        return Shaders.Add(shader);
    }

    void RenderFactoryOGL::Delete(CHEngine::ShaderHandle handle)
    {
        Shaders.Remove(handle);
    }

    // ============================================================
    // Texture
    // ============================================================

    CHEngine::TextureHandle RenderFactoryOGL::CreateTexture(
        const uint8_t* data,
        uint32_t width,
        uint32_t height,
        uint32_t channels,
        uint32_t mipLevels,
        uint32_t arrayLayers,
        CHEngine::TextureFormat format,
        CHEngine::TextureType type,
        CHEngine::TextureUsage usage,
        CHEngine::MemoryType memory,
        const String& debugName)
    {
        auto* tex = new TextureOGL(data, width, height, channels,
                                   mipLevels, arrayLayers,
                                   format, type, usage, memory,
                                   debugName.c_str());
        return Textures.Add(tex);
    }

    void RenderFactoryOGL::Delete(CHEngine::TextureHandle handle)
    {
        Textures.Remove(handle);
    }

    uint64_t RenderFactoryOGL::GetTextureNativeID(CHEngine::TextureHandle h)
    {
        TextureOGL* tex = Textures.Get(h);
        return tex ? static_cast<uint64_t>(tex->GetRendererID()) : 0u;
    }

    // ============================================================
    // Present blit (ImGui-less runtime)
    // ============================================================

    void RenderFactoryOGL::EnsurePresentResources()
    {
        if (m_PresentProgram != 0)
            return;

        // Fullscreen triangle generated from gl_VertexID — no vertex buffer needed.
        // FBO textures are stored bottom-up (same as the default framebuffer), so
        // no V-flip is applied here.
        static const char* kVS =
            "#version 410 core\n"
            "out vec2 vUV;\n"
            "void main(){\n"
            "  vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);\n"
            "  vUV = p;\n"
            "  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
            "}\n";
        static const char* kFS =
            "#version 410 core\n"
            "in vec2 vUV;\n"
            "out vec4 FragColor;\n"
            "uniform sampler2D uTex;\n"
            "void main(){ FragColor = texture(uTex, vUV); }\n";

        auto compile = [](GLenum type, const char* src) -> GLuint {
            GLuint s = glCreateShader(type);
            glShaderSource(s, 1, &src, nullptr);
            glCompileShader(s);
            GLint ok = 0;
            glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
            if (!ok) {
                char log[512];
                glGetShaderInfoLog(s, sizeof(log), nullptr, log);
                CHE_CORE_ERROR("Present blit shader compile failed: {}", log);
            }
            return s;
        };

        GLuint vs = compile(GL_VERTEX_SHADER, kVS);
        GLuint fs = compile(GL_FRAGMENT_SHADER, kFS);
        m_PresentProgram = glCreateProgram();
        glAttachShader(m_PresentProgram, vs);
        glAttachShader(m_PresentProgram, fs);
        glLinkProgram(m_PresentProgram);
        glDeleteShader(vs);
        glDeleteShader(fs);

        glGenVertexArrays(1, &m_PresentVAO);
    }

    void RenderFactoryOGL::PresentToBackbuffer(CHEngine::TextureHandle viewportTex,
                                               uint32_t w, uint32_t h)
    {
        TextureOGL* tex = Textures.Get(viewportTex);
        if (!tex)
            return;

        EnsurePresentResources();

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, static_cast<GLsizei>(w), static_cast<GLsizei>(h));
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);

        glUseProgram(m_PresentProgram);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex->GetRendererID());
        glUniform1i(glGetUniformLocation(m_PresentProgram, "uTex"), 0);

        glBindVertexArray(m_PresentVAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glBindVertexArray(0);
        glUseProgram(0);
    }

    // ============================================================
    // Buffer update
    // ============================================================

    void RenderFactoryOGL::UpdateBuffer(CHEngine::BufferHandle h,
                                        std::span<const std::byte> data,
                                        uint64_t offset)
    {
        BufferOGL* buf = Buffers.Get(h);
        if (buf)
            buf->UpdateData(data, offset);
    }

    // ============================================================
    // Shader hot-reload
    // ============================================================

    bool RenderFactoryOGL::ReloadShader(CHEngine::ShaderHandle h,
                                        const String& slangSource,
                                        const String& vertEntry,
                                        const String& fragEntry,
                                        const String& sourcePath)
    {
        ShaderOGL* shader = Shaders.Get(h);
        if (!shader) return false;
        bool ok = shader->Reload(slangSource, vertEntry, fragEntry, sourcePath);

        // PSO invalidation: rebuild all pipelines that depend on this shader.
        if (ok)
        {
            auto it = m_ShaderToPipelines.find(h.index);
            if (it != m_ShaderToPipelines.end())
            {
                for (CHEngine::PipelineHandle ph : it->second)
                {
                    PipelineOGL* pip = Pipelines.Get(ph);
                    if (pip)
                        pip->Rebuild(pip->GetDesc()); // re-apply with same desc (new shader)
                }
            }
        }
        return ok;
    }

    // ============================================================
    // Pipeline
    // ============================================================

    CHEngine::PipelineHandle RenderFactoryOGL::CreatePipeline(CHEngine::PipelineDesc desc)
    {
        auto* pip = new PipelineOGL(desc);
        CHEngine::PipelineHandle handle = Pipelines.Add(pip);

        // Register reverse index for PSO invalidation on shader reload.
        if (desc.Shader.IsValid())
            m_ShaderToPipelines[desc.Shader.index].push_back(handle);

        return handle;
    }

    void RenderFactoryOGL::Delete(CHEngine::PipelineHandle handle)
    {
        PipelineOGL* pip = Pipelines.Get(handle);
        if (pip)
        {
            // Remove from reverse index.
            CHEngine::ShaderHandle sh = pip->GetShaderHandle();
            if (sh.IsValid())
            {
                auto it = m_ShaderToPipelines.find(sh.index);
                if (it != m_ShaderToPipelines.end())
                {
                    auto& vec = it->second;
                    vec.erase(std::remove(vec.begin(), vec.end(), handle), vec.end());
                    if (vec.empty())
                        m_ShaderToPipelines.erase(it);
                }
            }
        }
        Pipelines.Remove(handle);
    }

    // ============================================================
    // Frame-graph backend
    // ============================================================

    std::unique_ptr<CHEngine::IFrameGraphBackend> RenderFactoryOGL::CreateFrameGraphBackend()
    {
        return std::make_unique<FrameGraphBackendOGL>(*this);
    }

    // ============================================================
    // Utility
    // ============================================================

    bool RenderFactoryOGL::CheckIsWorking()
    {
        return CHModules::CheckIsWorking();
    }

    void RenderFactoryOGL::Init(const CHEngine::RendererInitInfo& init)
    {
        Api.Init(init);
        m_UBOOffsetAlignment = Api.GetUniformBufferOffsetAlignment();

        if (!Slang.Init(CHEngine::ERenderAPI::OPENGL))
            CHE_CORE_ERROR("RenderFactoryOGL: failed to initialise SlangBackend for OpenGL");
    }

    void RenderFactoryOGL::Shutdown()
    {
        if (m_PresentVAO)     { glDeleteVertexArrays(1, &m_PresentVAO); m_PresentVAO = 0; }
        if (m_PresentProgram) { glDeleteProgram(m_PresentProgram);      m_PresentProgram = 0; }
        Api.Shutdown();
    }

} // namespace CHModules

IMPLEMENT_MODULE_FACTORY(CHModules::RenderFactoryOGL)
