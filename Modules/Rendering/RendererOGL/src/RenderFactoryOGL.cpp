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
        const CHEngine::String& debugName)
    {
        auto* buf = new BufferOGL(size, usage, memory, initialData, debugName);
        return Buffers.Add(buf);
    }

    void RenderFactoryOGL::Delete(CHEngine::BufferHandle handle)
    {
        if (auto* ptr = Buffers.Get(handle))
            delete ptr;
        Buffers.Remove(handle);
    }

    // ============================================================
    // Shader
    // ============================================================

    CHEngine::ShaderHandle RenderFactoryOGL::CreateShader(
        const CHEngine::String& slangSource,
        const CHEngine::String& vertEntry,
        const CHEngine::String& fragEntry,
        const CHEngine::String& sourcePath)
    {
        auto* shader = new ShaderOGL(slangSource, vertEntry, fragEntry, sourcePath);
        return Shaders.Add(shader);
    }

    void RenderFactoryOGL::Delete(CHEngine::ShaderHandle handle)
    {
        if (auto* ptr = Shaders.Get(handle))
            delete ptr;
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
        const char* debugName)
    {
        auto* tex = new TextureOGL(data, width, height, channels,
                                   mipLevels, arrayLayers,
                                   format, type, usage, memory,
                                   debugName);
        return Textures.Add(tex);
    }

    void RenderFactoryOGL::Delete(CHEngine::TextureHandle handle)
    {
        if (auto* ptr = Textures.Get(handle))
            delete ptr;
        Textures.Remove(handle);
    }

    uint64_t RenderFactoryOGL::GetTextureNativeID(CHEngine::TextureHandle h)
    {
        TextureOGL* tex = Textures.Get(h);
        return tex ? static_cast<uint64_t>(tex->GetRendererID()) : 0u;
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
                                        const CHEngine::String& slangSource,
                                        const CHEngine::String& vertEntry,
                                        const CHEngine::String& fragEntry,
                                        const CHEngine::String& sourcePath)
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
            delete pip;
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
    }

    void RenderFactoryOGL::Shutdown()
    {
        Api.Shutdown();
    }

} // namespace CHModules

IMPLEMENT_MODULE_FACTORY(CHModules::RenderFactoryOGL)
