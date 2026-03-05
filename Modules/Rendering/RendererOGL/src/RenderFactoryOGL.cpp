#include "RenderFactoryOGL.h"

#include "ImGuiLayerOGL.h"
#include "BufferOGL.h"
#include "VertexArrayOGL.h"
#include "ShaderOGL.h"
#include "RendererOGL.h"
#include "RenderApiOGL.h"

namespace CHModules
{
    CHEngine::IVertexBuffer* RenderFactoryOGL::CreateVertexBuffer(float* verticies, uint32_t size)
    {
        return CHEngine::CreateImpl<VertexBufferOGL>(verticies, size);
    }

    CHEngine::IIndexBuffer* RenderFactoryOGL::CreateIndexBuffer(uint32_t* indices, uint32_t count)
    {
        return CHEngine::CreateImpl<IndexBufferOGL>(indices, count);
    }

    CHEngine::IVertexArray* RenderFactoryOGL::CreateVertexArray()
    {
        return CHEngine::CreateImpl<VertexArrayOGL>();
    }

    CHEngine::IShader* RenderFactoryOGL::CreateShader(const CHEngine::String& vertexSrc, const CHEngine::String& fragmentSrc)
    {
        return CHEngine::CreateImpl<ShaderOGL>(vertexSrc, fragmentSrc);
    }

    CHEngine::RendererAPI* RenderFactoryOGL::CreateRenderAPI()
    {
        return CHEngine::CreateImpl<RendererApiOGL>();
    }

    CHEngine::IRenderer* RenderFactoryOGL::CreateRenderer(
        const unsigned int width,
        const unsigned int height,
        const char* title,
        CHEngine::ErrorCallbackFn errorCallbackFn)
    {
        return CHEngine::CreateImpl<RendererOGL>(width, height, title, errorCallbackFn);
    }

    CHEngine::IImGuiLayer* RenderFactoryOGL::CreateImGuiLayer(void* nativeWindow)
    {
        return CHEngine::CreateImpl<ImGuiLayerOGL>(nativeWindow);
    }

    CHEngine::ModuleType RenderFactoryOGL::GetType() const
    {
        return CHEngine::ModuleType::Render;
    }

    void RenderFactoryOGL::Delete(CHEngine::IVertexBuffer* ptr)
    {
        CHEngine::DestroyImpl(static_cast<VertexBufferOGL*>(ptr));
    }

    void RenderFactoryOGL::Delete(CHEngine::IIndexBuffer* ptr)
    {
        CHEngine::DestroyImpl(static_cast<IndexBufferOGL*>(ptr));
    }

    void RenderFactoryOGL::Delete(CHEngine::IVertexArray* ptr)
    {
        CHEngine::DestroyImpl(static_cast<VertexArrayOGL*>(ptr));
    }

    void RenderFactoryOGL::Delete(CHEngine::IShader* ptr)
    {
        CHEngine::DestroyImpl(static_cast<ShaderOGL*>(ptr));
    }

    void RenderFactoryOGL::Delete(CHEngine::RendererAPI* ptr)
    {
        CHEngine::DestroyImpl(static_cast<RendererApiOGL*>(ptr));
    }

    void RenderFactoryOGL::Delete(CHEngine::IRenderer* ptr)
    {
        CHEngine::DestroyImpl(static_cast<RendererOGL*>(ptr));
    }

    void RenderFactoryOGL::Delete(CHEngine::IImGuiLayer* ptr)
    {
        CHEngine::DestroyImpl(static_cast<ImGuiLayerOGL*>(ptr));
    }
}

IMPLEMENT_MODULE_FACTORY(CHModules::RenderFactoryOGL);