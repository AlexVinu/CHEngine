#pragma once

#include "UISystem/IImGuiFactory.h"
#include "ImGuiLayerOGL.h"

namespace CHModules {

    struct ImGuiFactoryOGL : CHEngine::IImGuiFactory
    {
        CHEngine::IImGuiLayer* CreateImGuiLayer(CHEngine::IWindow* window,
                                                CHEngine::IRenderFactory* renderFactory) override;

        void Delete(CHEngine::IImGuiLayer* ptr) override;

        CHEngine::ModuleType GetType() const override;
    };

}
