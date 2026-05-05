#pragma once

#include <Core.h>
#include "IModuleFactory.h"
#include "Render/UISystem/IImGuiLayer.h"
#include "WindowSystem/IWindow.h"

namespace CHEngine {
    struct IImGuiFactory : IModuleFactory
    {
        virtual ~IImGuiFactory() = default;

        // Принимает абстрактный IWindow* — модуль сам возьмёт GetNativeWindow() внутри.
        virtual IImGuiLayer* CreateImGuiLayer(IWindow* window) = 0;
        virtual void Delete(IImGuiLayer* ptr) = 0;
    };
}

DECLARE_MODULE_FACTORY()
