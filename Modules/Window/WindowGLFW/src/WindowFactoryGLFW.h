#pragma once

#include "WindowSystem/IWindowFactory.h"
#include "WindowGLFW.h"

namespace CHModules {

    struct WindowFactoryGLFW : CHEngine::IWindowFactory
    {
        CHEngine::IWindow* CreateIWindow(
            uint32_t width, uint32_t height,
            const char* title,
            CHEngine::ErrorCallbackFn errorCallbackFn, CHEngine::ERenderAPI renderApi) override;

        void Delete(CHEngine::IWindow* ptr) override;

        CHEngine::ModuleType GetType() const override;
    };

}
