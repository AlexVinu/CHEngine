#pragma once

#include "UISystem/IImGuiLayer.h"
#include "WindowSystem/IWindow.h"

namespace CHModules {

    class ImGuiLayerOGL : public CHEngine::IImGuiLayer
    {
    public:
        explicit ImGuiLayerOGL(CHEngine::IWindow* window);
        ~ImGuiLayerOGL() override;

        void Begin() override;
        void End() override;
    };

}
