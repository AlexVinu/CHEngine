#pragma once

#include <Core.h>
#include "IModuleFactory.h"
#include "UISystem/IImGuiLayer.h"
#include "WindowSystem/IWindow.h"

namespace CHEngine {
    struct IRenderFactory;

    struct IImGuiFactory : IModuleFactory
    {
        virtual ~IImGuiFactory() = default;

        // renderFactory обеспечивает доступ к нативному shared-контексту рендерера
        // (нужен ImGuiVK для VkInstance/Device/Queue). Остальные бэкенды его игнорируют.
        virtual IImGuiLayer* CreateImGuiLayer(IWindow* window, IRenderFactory* renderFactory) = 0;
        virtual void Delete(IImGuiLayer* ptr) = 0;
    };
}

DECLARE_MODULE_FACTORY()
