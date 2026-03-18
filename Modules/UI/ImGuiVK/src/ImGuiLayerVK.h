#pragma once

#include "UISystem/IImGuiLayer.h"
#include "WindowSystem/IWindow.h"

namespace CHModules {

    class ImGuiLayerVK : public CHEngine::IImGuiLayer
    {
    public:
        explicit ImGuiLayerVK(CHEngine::IWindow* window);
        ~ImGuiLayerVK() override;

        void Begin() override;
        void End() override;

    private:
        bool m_Initialized = false;
    };

}
