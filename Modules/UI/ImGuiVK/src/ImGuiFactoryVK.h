#pragma once

#include "UISystem/IImGuiFactory.h"
#include "ImGuiLayerVK.h"

namespace CHModules {

    struct ImGuiFactoryVK : CHEngine::IImGuiFactory
    {
        CHEngine::IImGuiLayer* CreateImGuiLayer(CHEngine::IWindow* window) override;
        void Delete(CHEngine::IImGuiLayer* ptr) override;
        CHEngine::ModuleType GetType() const override;
    };

}
