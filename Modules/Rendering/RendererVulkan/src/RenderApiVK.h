#pragma once

#include "Render/IRenderApi.h"
#include "VulkanContext.h"

namespace CHModules {

    class RenderApiVK : public CHEngine::IRenderApi
    {
    public:
        RenderApiVK() = default;
        ~RenderApiVK() override = default;

        void Init(const CHEngine::RendererInitInfo& init_info) override;
        void Shutdown() override;
        bool BeginFrame() override;
        void EndFrame() override;
        CHEngine::RenderContextInfo GetRenderContext() const override { return {}; }

        void SetClearColor(float r, float g, float b, float a) override;
        void Clear() override;

        void SetViewport(uint32_t width, uint32_t height) override;
        void SetBlend(bool enable) override;
        void SetDepthWrite(bool enable) override;

        void DrawIndexed(const CHEngine::IVertexArray* vertexArray) override;
        void DrawLines(const CHEngine::IVertexArray* vertexArray) override;

    private:
        VulkanContext m_Context;
    };

}
