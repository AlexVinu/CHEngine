#pragma once

#include "Render/IRenderer.h"
#include "MetalContext.h"

namespace CHModules
{
    class RendererMTL : public CHEngine::IRenderer
    {
    public:
        RendererMTL() = default;
        ~RendererMTL() override = default;

        void Init(const CHEngine::RendererInitInfo& init_info) override;
        void Shutdown() override;

        void SetViewport(uint32_t width, uint32_t height) override;

        bool BeginFrame() override;
        void EndFrame() override;
        CHEngine::RenderContextInfo GetRenderContext() const override;

        MetalContext& GetContext() { return m_Context; }

    private:
        MetalContext m_Context;
    };
}
