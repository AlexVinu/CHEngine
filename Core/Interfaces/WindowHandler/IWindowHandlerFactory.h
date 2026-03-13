#pragma once

#include <Core.h>

#include "IWindowHandler.h"
#include "IModuleFactory.h"
#include "WindowProps.h"

namespace CHEngine {

    struct IWindowHandlerFactory : IModuleFactory
    {
        virtual ~IWindowHandlerFactory() = default;

        virtual IWindowHandler* Create(const WindowProps& props, ErrorCallbackFn errorCallbackFn) = 0;
        virtual void Delete(IWindowHandler* handler) = 0;

        ModuleType GetType() const override { return ModuleType::WindowHandler; }
    };
}
