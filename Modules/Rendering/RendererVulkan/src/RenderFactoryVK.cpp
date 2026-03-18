#include "RenderFactoryVK.h"

#include "BufferVK.h"
#include "VertexArrayVK.h"
#include "ShaderVK.h"
#include "TextureVK.h"
#include "FramebufferVK.h"
#include "RendererVK.h"
#include "RenderApiVK.h"

namespace CHModules
{
    CHEngine::IVertexBuffer* RenderFactoryVK::CreateVertexBuffer(float* verticies, uint32_t size)
    {
        return CreateImpl<VertexBufferVK>(verticies, size);
    }

    CHEngine::IIndexBuffer* RenderFactoryVK::CreateIndexBuffer(uint32_t* indices, uint32_t count)
    {
        return CreateImpl<IndexBufferVK>(indices, count);
    }

    CHEngine::IVertexArray* RenderFactoryVK::CreateVertexArray()
    {
        return CreateImpl<VertexArrayVK>();
    }

    CHEngine::IShader* RenderFactoryVK::CreateShader(const CHEngine::String& vertexSrc, const CHEngine::String& fragmentSrc)
    {
        return CreateImpl<ShaderVK>(vertexSrc, fragmentSrc);
    }

    CHEngine::RendererAPI* RenderFactoryVK::CreateRenderAPI()
    {
        return CreateImpl<RenderApiVK>();
    }

    CHEngine::IRenderer* RenderFactoryVK::CreateRenderer(const CHEngine::RendererInitInfo& init_info)
    {
        auto* renderer = CreateImpl<RendererVK>();
        renderer->Init(init_info);
        return renderer;
    }

    CHEngine::ITexture* RenderFactoryVK::CreateTexture(const uint8_t* data, uint32_t width,
                                                        uint32_t height, uint32_t channels)
    {
        return CreateImpl<TextureVK>(data, width, height, channels);
    }

    CHEngine::IFramebuffer* RenderFactoryVK::CreateFramebuffer(uint32_t width, uint32_t height)
    {
        return new FramebufferVK(width, height);
    }

    void RenderFactoryVK::Delete(CHEngine::IVertexBuffer* ptr) { DestroyImpl(static_cast<VertexBufferVK*>(ptr)); }
    void RenderFactoryVK::Delete(CHEngine::IIndexBuffer*  ptr) { DestroyImpl(static_cast<IndexBufferVK*>(ptr)); }
    void RenderFactoryVK::Delete(CHEngine::IVertexArray*  ptr) { DestroyImpl(static_cast<VertexArrayVK*>(ptr)); }
    void RenderFactoryVK::Delete(CHEngine::IShader*       ptr) { DestroyImpl(static_cast<ShaderVK*>(ptr)); }
    void RenderFactoryVK::Delete(CHEngine::RendererAPI*   ptr) { DestroyImpl(static_cast<RenderApiVK*>(ptr)); }
    void RenderFactoryVK::Delete(CHEngine::IRenderer*     ptr) { DestroyImpl(static_cast<RendererVK*>(ptr)); }
    void RenderFactoryVK::Delete(CHEngine::ITexture*      ptr) { DestroyImpl(static_cast<TextureVK*>(ptr)); }
    void RenderFactoryVK::Delete(CHEngine::IFramebuffer*  ptr) { delete static_cast<FramebufferVK*>(ptr); }

    CHEngine::ModuleType RenderFactoryVK::GetType() const { return CHEngine::ModuleType::Render; }
    CHEngine::ERenderAPI RenderFactoryVK::GetRenderApi() { return CHEngine::ERenderAPI::VULKAN; }
}

IMPLEMENT_MODULE_FACTORY(CHModules::RenderFactoryVK)
