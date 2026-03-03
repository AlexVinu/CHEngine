#include "chepch.h"
#include "WindowsWindow.h"

#include "CHEngine/Events/ApplicationEvent.h"
#include "CHEngine/Events/MouseEvent.h"
#include "CHEngine/Events/KeyEvent.h"

namespace CHEngine {

	static void GLFWErrorCallback(int error, const char* description)
	{
		CHE_CORE_ERROR("GLFW Error ({0}): {1}", error, description);
	}

	Window* Window::Create(IRenderFactory* render_factory, const WindowProps& props)
	{
		return new WindowsWindow(props, render_factory);
	}

	WindowsWindow::WindowsWindow(const WindowProps& props, IRenderFactory* render_factory)
	{
		Init(props, render_factory);
	}

	WindowsWindow::~WindowsWindow()
	{
		Shutdown();
	}

	void WindowsWindow::OnUpdate()
	{
		//glfwPollEvents();
		m_Renderer->PollEvents();
		//glfwSwapBuffers(m_Window);
		m_Renderer->SwapBuffers();
	}

	void WindowsWindow::SetVSync(bool enabled)
	{
		m_Renderer->SetVSync(enabled);
		m_Data.VSync = enabled;
	}

	bool WindowsWindow::IsVSync() const
	{
		return m_Data.VSync;
	}

	void WindowsWindow::Init(const WindowProps& props, IRenderFactory* render_factory)
	{
		m_Data.Title = props.Title;
		m_Data.Width = props.Width;
		m_Data.Height = props.Height;

		CHE_CORE_INFO("Creating window {0}, ({1}, {2})", props.Title, props.Width, props.Height);
		m_Renderer = render_factory->CreateRenderer(props.Width, props.Height, props.Title.c_str(), GLFWErrorCallback);

		//m_Window = glfwCreateWindow((int)props.Width, (int)props.Height, m_Data.Title.c_str(), nullptr, nullptr);
		//glfwMakeContextCurrent(m_Window);
		//glfwSetWindowUserPointer(m_Window, &m_Data);
		//SetVSync(true);
		m_Renderer->SetVSync(true);

		RendererWindowContext ctx{};
		ctx.UserPointer = this; // WindowsWindow*

		ctx.ResizeCallback = [](void* user, int width, int height)
			{
				auto* window = (WindowsWindow*)user;
				window->m_Data.Width = width;
				window->m_Data.Height = height;

				WindowResizeEvent event(width, height);
				window->m_Data.EventCallback(event);
			};

		ctx.CloseCallback = [](void* user)
			{
				auto* window = (WindowsWindow*)user;
				WindowCloseEvent event;
				window->m_Data.EventCallback(event);
			};

		ctx.KeyCallback = [](void* user, int key, int scancode, int action, int mods)
			{
				auto* window = (WindowsWindow*)user;
				switch (action)
				{
					case (int)EventType::KeyPressed:
					{
						KeyPressedEvent event(key, 0);
						window->m_Data.EventCallback(event);
						break;
					}
					case (int)EventType::KeyReleased:
					{
						KeyReleasedEvent event(key);
						window->m_Data.EventCallback(event);
						break;
					}
					case (int)EventType::KeyRepeat:
					{
						KeyPressedEvent event(key, 1);
						window->m_Data.EventCallback(event);
						break;
					}
				}
			};

		ctx.MouseButtonCallback = [](void* user, int button, int action, int mods)
			{
				auto* window = (WindowsWindow*)user;

				switch (action)
				{
					case (int)EventType::KeyPressed:
					{
						MouseButtonPressedEvent event(button);
						window->m_Data.EventCallback(event);
						break;
					}
					case (int)EventType::KeyReleased:
					{
						MouseButtonReleasedEvent event(button);
						window->m_Data.EventCallback(event);
						break;
					}
				}
			};

		ctx.ScrollCallback = [](void* user, float xOffset, float yOffset)
			{
				auto* window = (WindowsWindow*)user;
				MouseScrolledEvent event(xOffset, yOffset);
				window->m_Data.EventCallback(event);
			};

		ctx.CursorPosCallback = [](void* user, float x, float y)
			{
				auto* window = (WindowsWindow*)user;
				MouseMovedEvent event(x, y);
				window->m_Data.EventCallback(event);
			};

		// Передаём в renderer
		m_Renderer->SetWindowContext(ctx);
	}

	void WindowsWindow::Shutdown()
	{
	}
}
