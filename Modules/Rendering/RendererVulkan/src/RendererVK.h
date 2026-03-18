#pragma once

#include "Render/IRenderer.h"
#include "VulkanContext.h"

namespace CHModules
{
    class RendererVK : public CHEngine::IRenderer
    {
    public:
        RendererVK() = default;
        ~RendererVK() override = default;

        void Init(const CHEngine::RendererInitInfo& init_info) override;
        void Shutdown() override;

        void SetViewport(uint32_t width, uint32_t height) override;

        VulkanContext& GetContext() { return m_Context; }

    private:
        VulkanContext m_Context;
    };
}
