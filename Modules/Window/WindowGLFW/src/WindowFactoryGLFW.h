#pragma once

#include "IWindowFactory.h"
#include "WindowGLFW.h"

namespace CHModules {

    struct WindowFactoryGLFW : CHEngine::IWindowFactory
    {
        CHEngine::IWindow* CreateIWindow(
            uint32_t width, uint32_t height,
            const char* title,
            CHEngine::ErrorCallbackFn errorCallbackFn) override
        {
            return CreateImpl<WindowGLFW>(width, height, title, errorCallbackFn);
        }

        void Delete(CHEngine::IWindow* ptr) override
        {
            DestroyImpl(static_cast<WindowGLFW*>(ptr));
        }

        CHEngine::ModuleType GetType() const override
        {
            return CHEngine::ModuleType::Window;
        }
    };

}
