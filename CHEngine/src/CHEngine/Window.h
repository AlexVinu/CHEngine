#pragma once

#include "chepch.h"

#include "Core.h"
#include "CHEngine/Events/Event.h"
#include "WindowSystem/IWindow.h"
#include "WindowSystem/IWindowFactory.h"

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
		virtual void SetMouse(bool) = 0;
		virtual bool IsMouse() const = 0;

        IWindow* GetPlatformWindow() const { return m_PlatformWindow; }

        static Window* Create(IWindowFactory* windowFactory, ERenderAPI renderApi = ERenderAPI::NONE, const WindowProps& props = WindowProps());

    protected:
        IWindow* m_PlatformWindow = nullptr;
        IWindowFactory* m_WindowFactory = nullptr;
    };
}
