#pragma once

#include "CHEngine/Window.h"

namespace CHEngine {

    class DesktopWindow : public Window
    {
    public:
        DesktopWindow(const WindowProps& props, IWindowFactory* windowFactory, ERenderAPI renderApi);
        virtual ~DesktopWindow();

        void OnUpdate() override;

        inline unsigned int GetWidth()  const override { return m_Data.Width; }
        inline unsigned int GetHeight() const override { return m_Data.Height; }

        inline void SetEventCallback(const EventCallbackFn& callback) override { m_Data.EventCallback = callback; }
        void SetVSync(bool enabled) override;
        bool IsVSync() const override;
		virtual void SetMouse(bool) override;
		virtual bool IsMouse() const override;

    private:
        void Init(const WindowProps& props, IWindowFactory* windowFactory, ERenderAPI renderApi);
        void Shutdown();

        struct WindowData
        {
            std::string Title;
            unsigned int Width, Height;
            bool VSync;
            bool Mouse;

            EventCallbackFn EventCallback;
        };

        WindowData      m_Data;
    };
}
