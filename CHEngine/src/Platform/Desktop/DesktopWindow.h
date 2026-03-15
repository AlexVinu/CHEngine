#pragma once

#include "CHEngine/Window.h"

namespace CHEngine {

    class DesktopWindow : public Window
    {
    public:
        DesktopWindow(const WindowProps& props, IWindowFactory* windowFactory);
        virtual ~DesktopWindow();

        void OnUpdate() override;

        inline unsigned int GetWidth()  const override { return m_Data.Width; }
        inline unsigned int GetHeight() const override { return m_Data.Height; }

        inline void SetEventCallback(const EventCallbackFn& callback) override { m_Data.EventCallback = callback; }
        void SetVSync(bool enabled) override;
        bool IsVSync() const override;
        void* GetNativeWindow() const override;
        IWindow* GetPlatformWindow() const override { return m_PlatformWindow; }

    private:
        void Init(const WindowProps& props, IWindowFactory* windowFactory);
        void Shutdown();

        struct WindowData
        {
            std::string Title;
            unsigned int Width, Height;
            bool VSync;

            EventCallbackFn EventCallback;
        };

        WindowData      m_Data;
        IWindow*        m_PlatformWindow = nullptr;
        IWindowFactory* m_WindowFactory  = nullptr;
    };
}
