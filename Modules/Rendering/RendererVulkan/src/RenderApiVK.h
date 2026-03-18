#pragma once

#include "Render/IRenderApi.h"

namespace CHModules {

    class RenderApiVK : public CHEngine::RendererAPI
    {
    public:
        RenderApiVK() = default;
        ~RenderApiVK() override = default;

        void SetClearColor(float r, float g, float b, float a) override;
        void Clear() override;

        void DrawIndexed(const CHEngine::IVertexArray* vertexArray) override;
        void DrawLines(const CHEngine::IVertexArray* vertexArray) override;
        void SetViewport(uint32_t width, uint32_t height) override;
        void SetBlend(bool enable) override;
        void SetDepthWrite(bool enable) override;

    private:
        float m_ClearR = 0.1f, m_ClearG = 0.1f, m_ClearB = 0.1f, m_ClearA = 1.0f;
    };

}
