#pragma once

#include "Render/IRenderFactory.h"
#include "FramebufferOGL.h"

namespace CHModules
{
    struct RenderFactoryOGL : public CHEngine::IRenderFactory
    {
        CHEngine::IVertexBuffer* CreateVertexBuffer(float* verticies, uint32_t size) override;
        CHEngine::IIndexBuffer*  CreateIndexBuffer(uint32_t* indices, uint32_t count) override;
        CHEngine::IVertexArray*  CreateVertexArray() override;
        CHEngine::IShader*       CreateShader(const CHEngine::String& vertexSrc, const CHEngine::String& fragmentSrc) override;
        CHEngine::IRenderApi*    CreateRenderAPI() override;

        CHEngine::IRenderer* CreateRenderer(CHEngine::IRenderApi* api) override;

        CHEngine::ITexture* CreateTexture(const uint8_t* data, uint32_t width,
                                          uint32_t height, uint32_t channels) override;

        CHEngine::IFramebuffer* CreateFramebuffer(uint32_t width, uint32_t height) override;

        void Delete(CHEngine::IVertexBuffer* ptr) override;
        void Delete(CHEngine::IIndexBuffer*  ptr) override;
        void Delete(CHEngine::IVertexArray*  ptr) override;
        void Delete(CHEngine::IShader*       ptr) override;
        void Delete(CHEngine::IRenderApi*    ptr) override;
        void Delete(CHEngine::IRenderer*     ptr) override;
        void Delete(CHEngine::ITexture*      ptr) override;
        void Delete(CHEngine::IFramebuffer*  ptr) override;

        CHEngine::ModuleType GetType() const override;

        virtual CHEngine::ERenderAPI GetRenderApi() override;

        virtual bool CheckIsWorking() override;
    };
}
