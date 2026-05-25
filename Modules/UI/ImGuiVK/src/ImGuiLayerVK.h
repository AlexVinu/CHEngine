#pragma once

#include "UISystem/IImGuiLayer.h"
#include "WindowSystem/IWindow.h"
#include "Render/IRenderFactory.h"
#include "RendererVK/VulkanSharedContext.h"
#include <vulkan/vulkan.h>

namespace CHModules {

    class ImGuiLayerVK : public CHEngine::IImGuiLayer
    {
    public:
        ImGuiLayerVK(CHEngine::IWindow* window, CHEngine::IRenderFactory* renderFactory);
        ~ImGuiLayerVK() override;

        void Begin() override;
        void End()   override;

    private:
        VulkanSharedContext*    m_VkCtx          = nullptr;
        bool                    m_Initialized    = false;
        PFN_vkCmdBeginRendering m_BeginRendering = nullptr;
        PFN_vkCmdEndRendering   m_EndRendering   = nullptr;
    };

}
