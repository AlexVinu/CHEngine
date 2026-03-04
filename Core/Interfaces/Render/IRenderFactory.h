#pragma once

#include <Core.h>
#include <Memory/String.h>

#include "IVertexArray.h"
#include "IShader.h"
#include "IRenderApi.h"
#include "IRenderer.h"
#include "IImGuiLayer.h"

#include "IModuleFactory.h"

namespace CHEngine {
    struct IRenderFactory : IModuleFactory
    {
        virtual ~IRenderFactory() = default;

        virtual IVertexBuffer* CreateVertexBuffer(float* verticies, uint32_t size) = 0;
        virtual IIndexBuffer* CreateIndexBuffer(uint32_t* indices, uint32_t count) = 0;
        virtual IVertexArray* CreateVertexArray() = 0;
        virtual IShader* CreateShader(const String& vertexSrc, const String& fragmentSrc) = 0;
        virtual RendererAPI* CreateRenderAPI() = 0;
        virtual IRenderer* CreateRenderer(const unsigned int width, const unsigned int height, const char* title, CHEngine::ErrorCallbackFn errorCallbackFn) = 0;

        virtual void Delete(IVertexBuffer* ptr) = 0;
        virtual void Delete(IIndexBuffer* ptr) = 0;
        virtual void Delete(IVertexArray* ptr) = 0;
        virtual void Delete(IShader* ptr) = 0;
        virtual void Delete(RendererAPI* ptr) = 0;
        virtual void Delete(IRenderer* ptr) = 0;

        virtual IImGuiLayer* CreateImGuiLayer(void* nativeWindow) = 0;
        virtual void Delete(IImGuiLayer* ptr) = 0;
    };
}

DECLARE_MODULE_FACTORY()