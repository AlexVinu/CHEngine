#pragma once

#include "chepch.h"

#include "Core.h"
#include "CHEngine/Events/Event.h"
#include "Render/IWindow.h"
#include "IWindowFactory.h"

namespace CHEngine {

    struct WindowProps
    {
        std::string Title;
        unsigned int Width;
        unsigned int Height;

        WindowProps(const std::string& title = "CHEngine",
            unsigned int width = 1280,
            unsigned int height = 720)
            : Title(title), Width(width), Height(height)
        {
        }
    };

    // Абстракция окна движка. Реализация — DesktopWindow, которая оборачивает IWindow.
    class CHENGINE_API Window
    {
    public:
        using EventCallbackFn = std::function<void(Event&)>;

        virtual ~Window() {}

        virtual void OnUpdate() = 0;

        virtual unsigned int GetWidth() const = 0;
        virtual unsigned int GetHeight() const = 0;

        virtual void SetEventCallback(const EventCallbackFn& callback) = 0;
        virtual void SetVSync(bool enabled) = 0;
        virtual bool IsVSync() const = 0;

        virtual void* GetNativeWindow() const = 0;

        // Возвращает низкоуровневый платформенный IWindow (GLFW и т.д.)
        // Используется Input::BeginFrame для прямого опроса состояния.
        virtual IWindow* GetPlatformWindow() const = 0;

        static Window* Create(IWindowFactory* windowFactory, const WindowProps& props = WindowProps());
    };
}
