#include "RenderFactoryOGL.h"

#include "BufferOGL.h"
#include "VertexArrayOGL.h"
#include "ShaderOGL.h"
#include "TextureOGL.h"
#include "RendererOGL.h"
#include "RenderApiOGL.h"

namespace CHModules
{
    CHEngine::IVertexBuffer* RenderFactoryOGL::CreateVertexBuffer(float* verticies, uint32_t size)
    {
        return CreateImpl<VertexBufferOGL>(verticies, size);
    }

    CHEngine::IIndexBuffer* RenderFactoryOGL::CreateIndexBuffer(uint32_t* indices, uint32_t count)
    {
        return CreateImpl<IndexBufferOGL>(indices, count);
    }

    CHEngine::IVertexArray* RenderFactoryOGL::CreateVertexArray()
    {
        return CreateImpl<VertexArrayOGL>();
    }

    CHEngine::IShader* RenderFactoryOGL::CreateShader(const CHEngine::String& vertexSrc, const CHEngine::String& fragmentSrc)
    {
        return CreateImpl<ShaderOGL>(vertexSrc, fragmentSrc);
    }

    CHEngine::RendererAPI* RenderFactoryOGL::CreateRenderAPI()
    {
        return CreateImpl<RendererApiOGL>();
    }

    CHEngine::IRenderer* RenderFactoryOGL::CreateRenderer(const CHEngine::RendererInitInfo& init_info)
    {
        auto* renderer = CreateImpl<RendererOGL>();
        renderer->Init(init_info);
        return renderer;
    }

    CHEngine::ModuleType RenderFactoryOGL::GetType() const
    {
        return CHEngine::ModuleType::Render;
    }

    CHEngine::ERenderAPI RenderFactoryOGL::GetRenderApi()
    {
        return CHEngine::ERenderAPI::OPENGL;
    }

    void RenderFactoryOGL::Delete(CHEngine::IVertexBuffer* ptr)
    {
        DestroyImpl(static_cast<VertexBufferOGL*>(ptr));
    }

    void RenderFactoryOGL::Delete(CHEngine::IIndexBuffer* ptr)
    {
        DestroyImpl(static_cast<IndexBufferOGL*>(ptr));
    }

    void RenderFactoryOGL::Delete(CHEngine::IVertexArray* ptr)
    {
        DestroyImpl(static_cast<VertexArrayOGL*>(ptr));
    }

    void RenderFactoryOGL::Delete(CHEngine::IShader* ptr)
    {
        DestroyImpl(static_cast<ShaderOGL*>(ptr));
    }

    void RenderFactoryOGL::Delete(CHEngine::RendererAPI* ptr)
    {
        DestroyImpl(static_cast<RendererApiOGL*>(ptr));
    }

    void RenderFactoryOGL::Delete(CHEngine::IRenderer* ptr)
    {
        DestroyImpl(static_cast<RendererOGL*>(ptr));
    }

    CHEngine::ITexture* RenderFactoryOGL::CreateTexture(const uint8_t* data, uint32_t width,
                                                         uint32_t height, uint32_t channels)
    {
        return CreateImpl<TextureOGL>(data, width, height, channels);
    }

    void RenderFactoryOGL::Delete(CHEngine::ITexture* ptr)
    {
        DestroyImpl(static_cast<TextureOGL*>(ptr));
    }

    CHEngine::IFramebuffer* RenderFactoryOGL::CreateFramebuffer(uint32_t width, uint32_t height)
    {
        return new FramebufferOGL(width, height);
    }

    void RenderFactoryOGL::Delete(CHEngine::IFramebuffer* ptr)
    {
        delete static_cast<FramebufferOGL*>(ptr);
    }
}

IMPLEMENT_MODULE_FACTORY(CHModules::RenderFactoryOGL)
