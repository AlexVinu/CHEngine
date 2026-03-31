#pragma once

#include <Core.h>
#include <Containers/String.h>

#include "IVertexArray.h"
#include "IShader.h"
#include "ITexture.h"
#include "IFramebuffer.h"
#include "IRenderApi.h"
#include "IRenderer.h"

#include "RenderData.h"

#include "IModuleFactory.h"

namespace CHEngine {
    struct IRenderFactory : IModuleFactory
    {
        virtual ~IRenderFactory() = default;

        virtual IVertexBuffer* CreateVertexBuffer(float* verticies, uint32_t size) = 0;
        virtual IIndexBuffer*  CreateIndexBuffer(uint32_t* indices, uint32_t count) = 0;
        virtual IVertexArray*  CreateVertexArray() = 0;
        virtual IShader*       CreateShader(const String& vertexSrc, const String& fragmentSrc) = 0;
        virtual IRenderApi*    CreateRenderAPI() = 0;

        virtual IRenderer* CreateRenderer(IRenderApi* api) = 0;

        // data — raw RGBA/RGB/RG/R pixels (уже декодированные stb_image или tinygltf).
        // channels: 1=R, 2=RG, 3=RGB, 4=RGBA.
        virtual ITexture* CreateTexture(const uint8_t* data, uint32_t width,
                                        uint32_t height, uint32_t channels) = 0;

        virtual IFramebuffer* CreateFramebuffer(uint32_t width, uint32_t height) = 0;

        virtual void Delete(IVertexBuffer* ptr) = 0;
        virtual void Delete(IIndexBuffer*  ptr) = 0;
        virtual void Delete(IVertexArray*  ptr) = 0;
        virtual void Delete(IShader*       ptr) = 0;
        virtual void Delete(IRenderApi*    ptr) = 0;
        virtual void Delete(IRenderer*     ptr) = 0;
        virtual void Delete(ITexture*      ptr) = 0;
        virtual void Delete(IFramebuffer*  ptr) = 0;

        virtual ERenderAPI GetRenderApi() = 0;

        virtual bool CheckIsWorking() = 0;
    };
}
