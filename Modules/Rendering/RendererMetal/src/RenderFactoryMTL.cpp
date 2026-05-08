#include "chepch.h"
#include "RenderFactoryMTL.h"

#include "FrameGraphBackendMTL.h"
#include "TextureMTL.h"    // Legacy TextureMTL
#include "RendererMTL.h"   // RendererMTL (legacy)
#include "CHEngine/UI/UIFacade.h"
#include <Log/Log.h>

namespace CHModules
{
    // ── New handle-based API ──────────────────────────────────────────────────

    CHEngine::BufferHandle RenderFactoryMTL::CreateBuffer(
        uint64_t size, CHEngine::BufferUsage usage, CHEngine::MemoryType memory,
        std::span<const std::byte> initialData, const CHEngine::String& /*debugName*/)
    {
        auto* buf = new UnifiedBufferMTL(size, usage, memory, initialData);
        return Buffers.Add(buf);
    }

    CHEngine::ShaderHandle RenderFactoryMTL::CreateShader(
        const CHEngine::String& slangSource,
        const CHEngine::String& vertEntry,
        const CHEngine::String& fragEntry,
        const CHEngine::String& sourcePath)
    {
        auto* sh = new ShaderMTL(slangSource, vertEntry, fragEntry, sourcePath);
        return Shaders.Add(sh);
    }

    CHEngine::TextureHandle RenderFactoryMTL::CreateTexture(
        const uint8_t* data,
        uint32_t width, uint32_t height, uint32_t channels,
        uint32_t /*mipLevels*/, uint32_t /*arrayLayers*/,
        CHEngine::TextureFormat format,
        CHEngine::TextureType   /*type*/,
        CHEngine::TextureUsage  usage,
        CHEngine::MemoryType    /*memory*/,
        const CHEngine::String& /*debugName*/)
    {
        TextureMTLFull* tex = nullptr;

        if (data != nullptr) {
            // Asset texture from raw pixel data
            tex = new TextureMTLFull(data, width, height, channels);
        } else {
            // Render target / depth buffer created from format descriptor
            tex = new TextureMTLFull(width, height, format, usage);
        }

        return Textures.Add(tex);
    }

    void RenderFactoryMTL::Delete(CHEngine::BufferHandle h)
    {
        auto* p = Buffers.Get(h);
        if (p) { delete p; Buffers.Remove(h); }
    }

    void RenderFactoryMTL::Delete(CHEngine::ShaderHandle h)
    {
        auto* p = Shaders.Get(h);
        if (p) { delete p; Shaders.Remove(h); }
    }

    void RenderFactoryMTL::Delete(CHEngine::TextureHandle h)
    {
        auto* p = Textures.Get(h);
        if (p) { delete p; Textures.Remove(h); }
    }

    void RenderFactoryMTL::Delete(CHEngine::PipelineHandle h)
    {
        auto* p = Pipelines.Get(h);
        if (p) { delete p; Pipelines.Remove(h); }
    }

    void RenderFactoryMTL::UpdateBuffer(CHEngine::BufferHandle h,
                                        std::span<const std::byte> data,
                                        uint64_t offset)
    {
        auto* buf = Buffers.Get(h);
        if (buf) buf->Update(data, offset);
    }

    uint64_t RenderFactoryMTL::GetTextureNativeID(CHEngine::TextureHandle h)
    {
        auto* tex = Textures.Get(h);
        return tex ? reinterpret_cast<uint64_t>(tex->GetNativeTexture()) : 0;
    }

    bool RenderFactoryMTL::ReloadShader(CHEngine::ShaderHandle h,
                                        const CHEngine::String& slangSource,
                                        const CHEngine::String& vertEntry,
                                        const CHEngine::String& fragEntry,
                                        const CHEngine::String& sourcePath)
    {
        auto* sh = Shaders.Get(h);
        if (!sh) return false;
        return sh->Reload(slangSource, vertEntry, fragEntry, sourcePath);
    }

    CHEngine::PipelineHandle RenderFactoryMTL::CreatePipeline(CHEngine::PipelineDesc desc)
    {
        auto* pipeline = new PipelineMTL(desc, *this);
        return Pipelines.Add(pipeline);
    }

    std::unique_ptr<CHEngine::IFrameGraphBackend> RenderFactoryMTL::CreateFrameGraphBackend()
    {
        return std::make_unique<FrameGraphBackendMTL>(*this);
    }

    // ── Legacy API ────────────────────────────────────────────────────────────

    void RenderFactoryMTL::Init(const CHEngine::RendererInitInfo& init)
    {
        // Create and initialize the Metal render API (sets up device, command queue, framebuffer)
        m_RenderApi = std::make_unique<RenderApiMTL>();
        m_RenderApi->Init(init);
    }

    void RenderFactoryMTL::Shutdown()
    {
        // Flush pools before destroying the Metal context
        Buffers.Clear();
        Textures.Clear();
        Shaders.Clear();
        Pipelines.Clear();

        if (m_RenderApi) {
            m_RenderApi->Shutdown();
            m_RenderApi.reset();
        }
    }

    void RenderFactoryMTL::BeginFrame()
    {
        if (!m_RenderApi) return;
        m_RenderApi->BeginFrame();
        // Provide ImGui with the current Metal command buffer + encoder
        CHEngine::UIFacade::SetRenderContext(m_RenderApi->GetRenderContext());
    }

    void RenderFactoryMTL::EndFrame()
    {
        if (!m_RenderApi) return;
        m_RenderApi->EndFrame();
    }

    CHEngine::IVertexBuffer* RenderFactoryMTL::CreateVertexBuffer(float* vertices, uint32_t size)
    { return CreateImpl<VertexBufferMTL>(vertices, size); }

    CHEngine::IIndexBuffer* RenderFactoryMTL::CreateIndexBuffer(uint32_t* indices, uint32_t count)
    { return CreateImpl<IndexBufferMTL>(indices, count); }

    CHEngine::IVertexArray* RenderFactoryMTL::CreateVertexArray()
    { return CreateImpl<VertexArrayMTL>(); }

    CHEngine::IRenderApi* RenderFactoryMTL::CreateRenderAPI()
    { return CreateImpl<RenderApiMTL>(); }

    void RenderFactoryMTL::Delete(CHEngine::IVertexBuffer* ptr) { DestroyImpl(static_cast<VertexBufferMTL*>(ptr)); }
    void RenderFactoryMTL::Delete(CHEngine::IIndexBuffer*  ptr) { DestroyImpl(static_cast<IndexBufferMTL*>(ptr)); }
    void RenderFactoryMTL::Delete(CHEngine::IVertexArray*  ptr) { DestroyImpl(static_cast<VertexArrayMTL*>(ptr)); }
    void RenderFactoryMTL::Delete(CHEngine::IRenderApi*    ptr) { DestroyImpl(static_cast<RenderApiMTL*>(ptr)); }
}

IMPLEMENT_MODULE_FACTORY(CHModules::RenderFactoryMTL)
