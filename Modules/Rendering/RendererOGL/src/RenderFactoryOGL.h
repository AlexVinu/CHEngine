#pragma once

#include "Render/IRenderFactory.h"

namespace CHModules
{
	struct RenderFactoryOGL : public CHEngine::IRenderFactory
	{
        virtual CHEngine::IVertexBuffer* CreateVertexBuffer(float* verticies, uint32_t size) override;
        virtual CHEngine::IIndexBuffer* CreateIndexBuffer(uint32_t* indices, uint32_t count) override;
        virtual CHEngine::IVertexArray* CreateVertexArray() override;
        virtual CHEngine::IShader* CreateShader(const CHEngine::String& vertexSrc, const CHEngine::String& fragmentSrc) override;
        virtual CHEngine::RendererAPI* CreateRenderAPI() override;
        virtual CHEngine::IRenderer* CreateRenderer(const unsigned int width, const unsigned int height, const char* title, CHEngine::ErrorCallbackFn errorCallbackFn) override;
	
        virtual void Delete(CHEngine::IVertexBuffer* ptr) override;
        virtual void Delete(CHEngine::IIndexBuffer* ptr) override;
        virtual void Delete(CHEngine::IVertexArray* ptr) override;
        virtual void Delete(CHEngine::IShader* ptr) override;
        virtual void Delete(CHEngine::RendererAPI* ptr) override;
        virtual void Delete(CHEngine::IRenderer* ptr) override;

        virtual CHEngine::IImGuiLayer* CreateImGuiLayer(void* nativeWindow) override;
        virtual void Delete(CHEngine::IImGuiLayer* ptr) override;

        virtual CHEngine::ModuleType GetType() const;
    };
}