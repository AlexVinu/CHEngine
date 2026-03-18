#pragma once

#include "UISystem/IImGuiLayer.h"
#include "WindowSystem/IWindow.h"

namespace CHModules {

    class ImGuiLayerMTL : public CHEngine::IImGuiLayer
    {
    public:
        explicit ImGuiLayerMTL(CHEngine::IWindow* window);
        ~ImGuiLayerMTL() override;

        void Begin() override;
        void End() override;

        void SetRenderContext(const CHEngine::RenderContextInfo& ctx) override;

    private:
        bool m_Initialized = false;
        bool m_MetalBackendReady = false;

        // Per-frame context from renderer
        void* m_CommandBuffer  = nullptr;
        void* m_RenderEncoder  = nullptr;
        void* m_RenderPassDesc = nullptr;
    };

}
