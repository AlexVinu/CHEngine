#pragma once

#include <Core.h>

#include <RenderData.h>
#include "Render/Core/RenderAPI.h"   // RenderContextInfo
#include "IVertexArray.h"

namespace CHEngine {

    class IRenderApi
    {
    public:
        virtual ~IRenderApi() = default;

        virtual void Init(const RendererInitInfo& init_info) = 0;
        virtual void Shutdown() = 0;

        virtual bool BeginFrame() { return true; }
        virtual void EndFrame() {}

        virtual RenderContextInfo GetRenderContext() const { return {}; }

        virtual void SetClearColor(float r, float g, float b, float a) = 0;
        virtual void Clear() = 0;

        virtual void SetViewport(uint32_t width, uint32_t height) = 0;
        virtual void SetBlend(bool enable) = 0;
        virtual void SetDepthWrite(bool enable) = 0;

        virtual void DrawIndexed(const IVertexArray* vertexArray) = 0;
        virtual void DrawLines(const IVertexArray* vertexArray) = 0;
    };

}
