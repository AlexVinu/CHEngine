#pragma once

#include "Render/IRenderApi.h"

namespace CHModules {

    class RenderApiMTL : public CHEngine::RendererAPI
    {
    public:
        RenderApiMTL();
        ~RenderApiMTL() override = default;

        void SetClearColor(float r, float g, float b, float a) override;
        void Clear() override;

        void DrawIndexed(const CHEngine::IVertexArray* vertexArray) override;
        void DrawLines(const CHEngine::IVertexArray* vertexArray) override;
        void SetViewport(uint32_t width, uint32_t height) override;
        void SetBlend(bool enable) override;
        void SetDepthWrite(bool enable) override;
    };

}
