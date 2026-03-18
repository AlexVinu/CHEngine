#include "RenderFactoryMTL.h"

#include "BufferMTL.h"
#include "VertexArrayMTL.h"
#include "ShaderMTL.h"
#include "TextureMTL.h"
#include "FramebufferMTL.h"
#include "RendererMTL.h"
#include "RenderApiMTL.h"

namespace CHModules
{
    CHEngine::IVertexBuffer* RenderFactoryMTL::CreateVertexBuffer(float* verticies, uint32_t size)
    { return CreateImpl<VertexBufferMTL>(verticies, size); }

    CHEngine::IIndexBuffer* RenderFactoryMTL::CreateIndexBuffer(uint32_t* indices, uint32_t count)
    { return CreateImpl<IndexBufferMTL>(indices, count); }

    CHEngine::IVertexArray* RenderFactoryMTL::CreateVertexArray()
    { return CreateImpl<VertexArrayMTL>(); }

    CHEngine::IShader* RenderFactoryMTL::CreateShader(const CHEngine::String& vertexSrc, const CHEngine::String& fragmentSrc)
    { return CreateImpl<ShaderMTL>(vertexSrc, fragmentSrc); }

    CHEngine::RendererAPI* RenderFactoryMTL::CreateRenderAPI()
    { return CreateImpl<RenderApiMTL>(); }

    CHEngine::IRenderer* RenderFactoryMTL::CreateRenderer(const CHEngine::RendererInitInfo& init_info)
    {
        auto* renderer = CreateImpl<RendererMTL>();
        renderer->Init(init_info);
        return renderer;
    }

    CHEngine::ITexture* RenderFactoryMTL::CreateTexture(const uint8_t* data, uint32_t width,
                                                         uint32_t height, uint32_t channels)
    { return CreateImpl<TextureMTL>(data, width, height, channels); }

    CHEngine::IFramebuffer* RenderFactoryMTL::CreateFramebuffer(uint32_t width, uint32_t height)
    { return new FramebufferMTL(width, height); }

    void RenderFactoryMTL::Delete(CHEngine::IVertexBuffer* ptr) { DestroyImpl(static_cast<VertexBufferMTL*>(ptr)); }
    void RenderFactoryMTL::Delete(CHEngine::IIndexBuffer*  ptr) { DestroyImpl(static_cast<IndexBufferMTL*>(ptr)); }
    void RenderFactoryMTL::Delete(CHEngine::IVertexArray*  ptr) { DestroyImpl(static_cast<VertexArrayMTL*>(ptr)); }
    void RenderFactoryMTL::Delete(CHEngine::IShader*       ptr) { DestroyImpl(static_cast<ShaderMTL*>(ptr)); }
    void RenderFactoryMTL::Delete(CHEngine::RendererAPI*   ptr) { DestroyImpl(static_cast<RenderApiMTL*>(ptr)); }
    void RenderFactoryMTL::Delete(CHEngine::IRenderer*     ptr) { DestroyImpl(static_cast<RendererMTL*>(ptr)); }
    void RenderFactoryMTL::Delete(CHEngine::ITexture*      ptr) { DestroyImpl(static_cast<TextureMTL*>(ptr)); }
    void RenderFactoryMTL::Delete(CHEngine::IFramebuffer*  ptr) { delete static_cast<FramebufferMTL*>(ptr); }

    CHEngine::ModuleType RenderFactoryMTL::GetType() const { return CHEngine::ModuleType::Render; }
    CHEngine::ERenderAPI RenderFactoryMTL::GetRenderApi() { return CHEngine::ERenderAPI::METALL; }
}

IMPLEMENT_MODULE_FACTORY(CHModules::RenderFactoryMTL)
