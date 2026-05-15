#pragma once

#include "MetalContext.h"
#include <Render/Core/RenderAPI.h>

namespace CHModules {

    // Legacy render API for old EditorViewport path
    // Note: Doesn't inherit from IRenderApi anymore as that interface was removed
    
    class RenderApiMTL
    {
    public:
        RenderApiMTL();
        ~RenderApiMTL();

        void Init(const struct CHEngine::RendererInitInfo& init_info);
        void Shutdown();
        bool BeginFrame();
        void EndFrame();
        struct CHEngine::RenderContextInfo GetRenderContext() const;

        void SetClearColor(float r, float g, float b, float a);
        void Clear();

        void SetViewport(uint32_t width, uint32_t height);
        void SetBlend(bool enable);
        void SetDepthWrite(bool enable);

        void DrawIndexed(class VertexArrayMTL* vertexArray);
        void DrawLines(class VertexArrayMTL* vertexArray);

    private:
        void UpdateDepthStencilState();

        MetalContext m_Context;
        void* m_DepthStencilState = nullptr;   // id<MTLDepthStencilState>
    };

}
