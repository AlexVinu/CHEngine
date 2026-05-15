#include "RenderFactoryVK.h"

#include "BufferVK.h"
#include "VertexArrayVK.h"
#include "ShaderVK.h"
#include "TextureVK.h"
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

    CHEngine::IShader* RenderFactoryVK::CreateShader(const String& slangSource,
                                                     const String& vertEntry,
                                                     const String& fragEntry,
                                                     const String& sourcePath)
    {
        return CreateImpl<ShaderVK>(slangSource, vertEntry, fragEntry, sourcePath);
    }

    CHEngine::IRenderApi* RenderFactoryVK::CreateRenderAPI()
    {
        return CreateImpl<RenderApiVK>();
    }

    CHEngine::IRenderer* RenderFactoryVK::CreateRenderer(CHEngine::IRenderApi* api)
    {
        return CreateImpl<RendererVK>(api);
    }

    CHEngine::ITexture* RenderFactoryVK::CreateTexture(const uint8_t* data, uint32_t width,
                                                        uint32_t height, uint32_t channels)
    {
        return CreateImpl<TextureVK>(data, width, height, channels);
    }

    void RenderFactoryVK::Delete(CHEngine::IVertexBuffer* ptr) { DestroyImpl(static_cast<VertexBufferVK*>(ptr)); }
    void RenderFactoryVK::Delete(CHEngine::IIndexBuffer*  ptr) { DestroyImpl(static_cast<IndexBufferVK*>(ptr)); }
    void RenderFactoryVK::Delete(CHEngine::IVertexArray*  ptr) { DestroyImpl(static_cast<VertexArrayVK*>(ptr)); }
    void RenderFactoryVK::Delete(CHEngine::IShader*       ptr) { DestroyImpl(static_cast<ShaderVK*>(ptr)); }
    void RenderFactoryVK::Delete(CHEngine::IRenderApi*    ptr) { DestroyImpl(static_cast<RenderApiVK*>(ptr)); }
    void RenderFactoryVK::Delete(CHEngine::IRenderer*     ptr) { DestroyImpl(static_cast<RendererVK*>(ptr)); }
    void RenderFactoryVK::Delete(CHEngine::ITexture*      ptr) { DestroyImpl(static_cast<TextureVK*>(ptr)); }

    CHEngine::ModuleType RenderFactoryVK::GetType() const { return CHEngine::ModuleType::Render; }
    CHEngine::ERenderAPI RenderFactoryVK::GetRenderApi() { return CHEngine::ERenderAPI::VULKAN; }
    bool RenderFactoryVK::CheckIsWorking() { return true; }
}

IMPLEMENT_MODULE_FACTORY(CHModules::RenderFactoryVK)
