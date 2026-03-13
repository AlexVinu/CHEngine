#include "chepch.h"
#include "DesktopWindow.h"

#include "CHEngine/Events/ApplicationEvent.h"
#include "CHEngine/Events/MouseEvent.h"
#include "CHEngine/Events/KeyEvent.h"

namespace CHEngine {

    static void GLFWErrorCallback(int error, const char* description)
    {
        CHE_CORE_ERROR("GLFW Error ({0}): {1}", error, description);
    }

    Window* Window::Create(IWindowFactory* windowFactory, const WindowProps& props)
    {
        return new DesktopWindow(props, windowFactory);
    }

    DesktopWindow::DesktopWindow(const WindowProps& props, IWindowFactory* windowFactory)
    {
        Init(props, windowFactory);
    }

    DesktopWindow::~DesktopWindow()
    {
        Shutdown();
    }

    void DesktopWindow::OnUpdate()
    {
        m_PlatformWindow->PollEvents();
        m_PlatformWindow->SwapBuffers();
    }

    void DesktopWindow::SetVSync(bool enabled)
    {
        m_PlatformWindow->SetVSync(enabled);
        m_Data.VSync = enabled;
    }

    bool DesktopWindow::IsVSync() const
    {
        return m_Data.VSync;
    }

    void DesktopWindow::Init(const WindowProps& props, IWindowFactory* windowFactory)
    {
        m_Data.Title  = props.Title;
        m_Data.Width  = props.Width;
        m_Data.Height = props.Height;
        m_Data.VSync  = true;
        m_WindowFactory = windowFactory;

        CHE_CORE_INFO("Creating window {0} ({1}, {2})", props.Title, props.Width, props.Height);

        m_PlatformWindow = windowFactory->CreateWindow(
            props.Width, props.Height,
            props.Title.c_str(),
            GLFWErrorCallback);

        WindowContext ctx{};
        ctx.UserPointer = this;

        ctx.ResizeCallback = [](void* user, int width, int height) {
            auto* window = (DesktopWindow*)user;
            window->m_Data.Width  = width;
            window->m_Data.Height = height;
            WindowResizeEvent event(width, height);
            window->m_Data.EventCallback(event);
        };

        ctx.CloseCallback = [](void* user) {
            auto* window = (DesktopWindow*)user;
            WindowCloseEvent event;
            window->m_Data.EventCallback(event);
        };

        ctx.KeyCallback = [](void* user, int key, int scancode, int action, int mods) {
            auto* window = (DesktopWindow*)user;
            switch (action)
            {
            case (int)EventType::KeyPressed: {
                KeyPressedEvent event(key, 0);
                window->m_Data.EventCallback(event);
                break;
            }
            case (int)EventType::KeyReleased: {
                KeyReleasedEvent event(key);
                window->m_Data.EventCallback(event);
                break;
            }
            case (int)EventType::KeyRepeat: {
                KeyPressedEvent event(key, 1);
                window->m_Data.EventCallback(event);
                break;
            }
            }
        };

        ctx.MouseButtonCallback = [](void* user, int button, int action, int mods) {
            auto* window = (DesktopWindow*)user;
            switch (action)
            {
            case (int)EventType::KeyPressed: {
                MouseButtonPressedEvent event(button);
                window->m_Data.EventCallback(event);
                break;
            }
            case (int)EventType::KeyReleased: {
                MouseButtonReleasedEvent event(button);
                window->m_Data.EventCallback(event);
                break;
            }
            }
        };

        ctx.ScrollCallback = [](void* user, float xOffset, float yOffset) {
            auto* window = (DesktopWindow*)user;
            MouseScrolledEvent event(xOffset, yOffset);
            window->m_Data.EventCallback(event);
        };

        ctx.CursorPosCallback = [](void* user, float x, float y) {
            auto* window = (DesktopWindow*)user;
            MouseMovedEvent event(x, y);
            window->m_Data.EventCallback(event);
        };

        m_PlatformWindow->SetWindowContext(ctx);
    }

    void* DesktopWindow::GetNativeWindow() const
    {
        return m_PlatformWindow->GetNativeWindow();
    }

    void DesktopWindow::Shutdown()
    {
        if (m_PlatformWindow && m_WindowFactory) {
            m_WindowFactory->Delete(m_PlatformWindow);
            m_PlatformWindow = nullptr;
        }
    }
}
