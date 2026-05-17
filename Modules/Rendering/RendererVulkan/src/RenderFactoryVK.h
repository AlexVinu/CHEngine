#pragma once

#include "Render/IRenderFactory.h"
#include <SlangBackend/SlangBackend.h>

namespace CHModules
{
    struct RenderFactoryVK : public CHEngine::IRenderFactory
    {
        // Slang shader compiler — lazily initialised on first CreateShader() call.
        CHModules::SlangBackend Slang;
        CHEngine::IVertexBuffer* CreateVertexBuffer(float* verticies, uint32_t size) override;
        CHEngine::IIndexBuffer*  CreateIndexBuffer(uint32_t* indices, uint32_t count) override;
        CHEngine::IVertexArray*  CreateVertexArray() override;
        CHEngine::IShader*       CreateShader(const String& slangSource,
                                               const String& vertEntry  = String("vertMain"),
                                               const String& fragEntry  = String("fragMain"),
                                               const String& sourcePath = String()) override;
        CHEngine::IRenderApi*    CreateRenderAPI() override;

        CHEngine::IRenderer* CreateRenderer(CHEngine::IRenderApi* api) override;

        CHEngine::ITexture* CreateTexture(const uint8_t* data, uint32_t width,
                                          uint32_t height, uint32_t channels) override;

        void Delete(CHEngine::IVertexBuffer* ptr) override;
        void Delete(CHEngine::IIndexBuffer*  ptr) override;
        void Delete(CHEngine::IVertexArray*  ptr) override;
        void Delete(CHEngine::IShader*       ptr) override;
        void Delete(CHEngine::IRenderApi*    ptr) override;
        void Delete(CHEngine::IRenderer*     ptr) override;
        void Delete(CHEngine::ITexture*      ptr) override;

        CHEngine::ModuleType GetType() const override;
        CHEngine::ERenderAPI GetRenderApi() override;
        bool CheckIsWorking() override;
    };
}
