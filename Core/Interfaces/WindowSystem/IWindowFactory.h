#pragma once

#include <Core.h>
#include "IModuleFactory.h"
#include "WindowSystem/IWindow.h"
#include "RenderData.h"

namespace CHEngine {
    struct IWindowFactory : IModuleFactory
    {
        virtual ~IWindowFactory() = default;

        virtual IWindow* CreateIWindow(
            uint32_t width, uint32_t height,
            const char* title,
            ErrorCallbackFn errorCallbackFn, ERenderAPI renderApi) = 0;

        virtual void Delete(IWindow* ptr) = 0;
    };
}

