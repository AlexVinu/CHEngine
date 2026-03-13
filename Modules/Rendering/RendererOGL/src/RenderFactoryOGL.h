#pragma once

#include "Render/IRenderFactory.h"

namespace CHModules
{
    struct RenderFactoryOGL : public CHEngine::IRenderFactory
    {
        CHEngine::IVertexBuffer* CreateVertexBuffer(float* verticies, uint32_t size) override;
        CHEngine::IIndexBuffer*  CreateIndexBuffer(uint32_t* indices, uint32_t count) override;
        CHEngine::IVertexArray*  CreateVertexArray() override;
        CHEngine::IShader*       CreateShader(const CHEngine::String& vertexSrc, const CHEngine::String& fragmentSrc) override;
        CHEngine::RendererAPI*   CreateRenderAPI() override;

        // Принимает нативное окно (GLFWwindow* из WindowGLFW) и инициализирует GLAD
        CHEngine::IRenderer* CreateRenderer(void* nativeWindow) override;

        CHEngine::ITexture* CreateTexture(const uint8_t* data, uint32_t width,
                                          uint32_t height, uint32_t channels) override;

        void Delete(CHEngine::IVertexBuffer* ptr) override;
        void Delete(CHEngine::IIndexBuffer*  ptr) override;
        void Delete(CHEngine::IVertexArray*  ptr) override;
        void Delete(CHEngine::IShader*       ptr) override;
        void Delete(CHEngine::RendererAPI*   ptr) override;
        void Delete(CHEngine::IRenderer*     ptr) override;
        void Delete(CHEngine::ITexture*      ptr) override;

        CHEngine::ModuleType GetType() const override;
    };
}
