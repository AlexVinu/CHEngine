#pragma once

#include <Core.h>
#include "EventData.h"

namespace CHEngine
{
    namespace WindowCallbacks {
        using ErrorCallbackFn       = void(*)(int, const char*);
        using ResizeCallbackFn      = void(*)(void*, int, int);
        using CloseCallbackFn       = void(*)(void*);
        using KeyCallbackFn         = void(*)(void*, int, int, int, int);
        using MouseButtonCallbackFn = void(*)(void*, int, int, int);
        using ScrollCallbackFn      = void(*)(void*, float, float);
        using CursorPosCallbackFn   = void(*)(void*, float, float);
    }

    using namespace WindowCallbacks;

    // Контекст коллбэков окна (аналог старого RendererWindowContext)
    struct WindowContext
    {
        void* UserPointer = nullptr;

        ResizeCallbackFn       ResizeCallback      = nullptr;
        CloseCallbackFn        CloseCallback       = nullptr;
        KeyCallbackFn          KeyCallback         = nullptr;
        MouseButtonCallbackFn  MouseButtonCallback = nullptr;
        ScrollCallbackFn       ScrollCallback      = nullptr;
        CursorPosCallbackFn    CursorPosCallback   = nullptr;
    };

    // Платформенное окно (GLFW, Win32 и т.д.)
    // Управляет созданием окна, OpenGL-контекстом, событиями и swap buffers
    class IWindow
    {
    public:
        virtual ~IWindow() = default;

        virtual void Shutdown() = 0;

        virtual void SwapBuffers() = 0;
        virtual void PollEvents() = 0;

        virtual void SetVSync(bool enabled) = 0;

        virtual void SetWindowContext(const WindowContext& context) = 0;

        virtual void* GetNativeWindow() = 0;

        virtual uint32_t GetWidth() const = 0;
        virtual uint32_t GetHeight() const = 0;
    };
}
