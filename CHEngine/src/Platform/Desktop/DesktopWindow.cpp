#include "chepch.h"
#include "DesktopWindow.h"

#include "CHEngine/Events/ApplicationEvent.h"

namespace CHEngine {

    static void GLFWErrorCallback(int error, const char* description)
    {
        CHE_CORE_ERROR("GLFW Error ({0}): {1}", error, description);
    }

    Window* Window::Create(IWindowFactory* windowFactory, ERenderAPI renderApi, const WindowProps& props)
    {
        return new DesktopWindow(props, windowFactory, renderApi);
    }

    DesktopWindow::DesktopWindow(const WindowProps& props, IWindowFactory* windowFactory, ERenderAPI renderApi)
    {
        Init(props, windowFactory, renderApi);
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

	void DesktopWindow::SetMouse(bool active)
	{
        m_PlatformWindow->SetMouse(active);
        m_Data.Mouse = active;
	}

	bool DesktopWindow::IsMouse() const
	{
        return m_Data.Mouse;
	}

	void DesktopWindow::Init(const WindowProps& props, IWindowFactory* windowFactory, ERenderAPI renderApi)
    {
        m_Data.Title  = props.Title;
        m_Data.Width  = props.Width;
        m_Data.Height = props.Height;
        m_Data.VSync  = true;
        m_Data.Mouse = true;
        m_WindowFactory = windowFactory;

        CHE_CORE_INFO("Creating window {0} ({1}, {2})", props.Title, props.Width, props.Height);

        m_PlatformWindow = windowFactory->CreateIWindow(
            props.Width, props.Height,
            props.Title.c_str(),
            GLFWErrorCallback, renderApi);

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

        // Клавиатура/мышь не транслируются в события: ввод читается через
        // polling (Input / InputSystem). Сюда приходят только оконные события.
        m_PlatformWindow->SetWindowContext(ctx);
    }

    void DesktopWindow::Shutdown()
    {
        if (m_PlatformWindow && m_WindowFactory) {
            m_WindowFactory->Delete(m_PlatformWindow);
            m_PlatformWindow = nullptr;
        }
    }
}
