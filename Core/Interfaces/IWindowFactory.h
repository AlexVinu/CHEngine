#pragma once

#include <Core.h>
#include "IModuleFactory.h"
#include "Render/IWindow.h"

namespace CHEngine {
    struct IWindowFactory : IModuleFactory
    {
        virtual ~IWindowFactory() = default;

        virtual IWindow* CreateIWindow(
            uint32_t width, uint32_t height,
            const char* title,
            ErrorCallbackFn errorCallbackFn) = 0;

        virtual void Delete(IWindow* ptr) = 0;
    };
}

DECLARE_MODULE_FACTORY()
