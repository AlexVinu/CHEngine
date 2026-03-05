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

    CHEngine::IRenderer* RenderFactoryOGL::CreateRenderer(
        const unsigned int width,
        const unsigned int height,
        const char* title,
        CHEngine::ErrorCallbackFn errorCallbackFn)
    {
        return CreateImpl<RendererOGL>(width, height, title, errorCallbackFn);
    }

    CHEngine::IImGuiLayer* RenderFactoryOGL::CreateImGuiLayer(void* nativeWindow)
    {
        return CreateImpl<ImGuiLayerOGL>(nativeWindow);
    }

    CHEngine::ModuleType RenderFactoryOGL::GetType() const
    {
        return CHEngine::ModuleType::Render;
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

    void RenderFactoryOGL::Delete(CHEngine::IImGuiLayer* ptr)
    {
        DestroyImpl(static_cast<ImGuiLayerOGL*>(ptr));
    }
}

IMPLEMENT_MODULE_FACTORY(CHModules::RenderFactoryOGL);