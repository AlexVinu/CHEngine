#pragma once

#include "WindowHandler/IWindowHandlerFactory.h"
#include "WindowHandlerGLFW.h"

namespace CHModules
{
    struct WindowHandlerFactoryGLFW : public CHEngine::IWindowHandlerFactory
    {
        virtual ~WindowHandlerFactoryGLFW() = default;

        virtual CHEngine::IWindowHandler* Create(const CHEngine::WindowProps& props,
                                                 CHEngine::ErrorCallbackFn errorCallbackFn) override;

        virtual void Delete(CHEngine::IWindowHandler* handler) override;

        virtual CHEngine::ModuleType GetType() const override;
    };
}

