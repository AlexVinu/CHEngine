#include "chepch.h"
#include "Application.h"

#include "Log/Log.h"

#include <filesystem>
#if defined(CHE_PLATFORM_APPLE)
#include <mach-o/dyld.h>
#endif

namespace CHEngine {

#define BIND_EVENT_FN(x) std::bind(&Application::x, this, std::placeholders::_1)

	Application* Application::s_Instance = nullptr;

	Application::Application()
		:m_ModuleManager()
	{
		CHE_CORE_ASSERT(!s_Instance, "Application already exists!");

#if defined(CHE_PLATFORM_APPLE)
		{
			char exePath[4096];
			uint32_t size = sizeof(exePath);
			if (_NSGetExecutablePath(exePath, &size) == 0)
			{
				auto exeDir = std::filesystem::path(exePath).parent_path();
				std::filesystem::current_path(exeDir);
				CHE_CORE_INFO("Working directory set to: {0}", exeDir.string().c_str());
			}
		}
#endif
		s_Instance = this;

		#if defined(CHE_PLATFORM_WINDOWS)
		m_ModuleManager.LoadModule("RendererOGL.dll");
		m_ModuleManager.LoadModule("WindowHandlerGLFW.dll");
		m_ModuleManager.LoadModule("ImGuiGLFW_OGL.dll");
		#elif defined(CHE_PLATFORM_APPLE)
		m_ModuleManager.LoadModule("libRendererOGL.dylib");
		m_ModuleManager.LoadModule("libWindowHandlerGLFW.dylib");
		m_ModuleManager.LoadModule("libImGuiGLFW_OGL.dylib");
		#else
		m_ModuleManager.LoadModule("libRendererOGL.so");
		m_ModuleManager.LoadModule("libWindowHandlerGLFW.so");
		m_ModuleManager.LoadModule("libImGuiGLFW_OGL.so");
		#endif

		m_RenderFactory        = m_ModuleManager.GetModule<IRenderFactory>(ModuleType::Renderer);
		m_WindowHandlerFactory = m_ModuleManager.GetModule<IWindowHandlerFactory>(ModuleType::WindowHandler);
		m_ImGuiFactory         = m_ModuleManager.GetModule<IImGuiFactory>(ModuleType::ImGui);

		m_RenderResources.Init(m_RenderFactory);

		WindowProps props;
		m_WindowHandler = m_WindowHandlerFactory->Create(props, nullptr);

		// Пробрасываем события окна напрямую в Application::OnEvent
		if (m_WindowHandler)
		{
			RendererWindowContext ctx{};
			ctx.UserPointer = this;

			ctx.ResizeCallback = [](void* user, int width, int height)
				{
					auto* app = static_cast<Application*>(user);
					WindowResizeEvent event(width, height);
					app->OnEvent(event);
				};

			ctx.CloseCallback = [](void* user)
				{
					auto* app = static_cast<Application*>(user);
					WindowCloseEvent event;
					app->OnEvent(event);
				};

			ctx.KeyCallback = [](void* user, int key, int scancode, int action, int mods)
				{
					auto* app = static_cast<Application*>(user);
					switch (action)
					{
					case (int)EventType::KeyPressed:
					{
						KeyPressedEvent event(key, 0);
						app->OnEvent(event);
						break;
					}
					case (int)EventType::KeyReleased:
					{
						KeyReleasedEvent event(key);
						app->OnEvent(event);
						break;
					}
					case (int)EventType::KeyRepeat:
					{
						KeyPressedEvent event(key, 1);
						app->OnEvent(event);
						break;
					}
					}
				};

			ctx.MouseButtonCallback = [](void* user, int button, int action, int mods)
				{
					auto* app = static_cast<Application*>(user);

					switch (action)
					{
					case (int)EventType::KeyPressed:
					{
						MouseButtonPressedEvent event(button);
						app->OnEvent(event);
						break;
					}
					case (int)EventType::KeyReleased:
					{
						MouseButtonReleasedEvent event(button);
						app->OnEvent(event);
						break;
					}
					}
				};

			ctx.ScrollCallback = [](void* user, float xOffset, float yOffset)
				{
					auto* app = static_cast<Application*>(user);
					MouseScrolledEvent event(xOffset, yOffset);
					app->OnEvent(event);
				};

			ctx.CursorPosCallback = [](void* user, float x, float y)
				{
					auto* app = static_cast<Application*>(user);
					MouseMovedEvent event(x, y);
					app->OnEvent(event);
				};

			m_WindowHandler->SetWindowContext(ctx);
			m_WindowHandler->SetVSync(true);
		}

		m_ImGuiLayer = m_ImGuiFactory->Create(m_WindowHandler ? m_WindowHandler->GetNativeWindow() : nullptr);

		m_RenderApi = m_RenderResources.CreateRenderAPI();
		// Background matches UITheme bg (#F2F2F7 light) — perceived as mid-grey in 3D
		m_RenderResources.Get(m_RenderApi)->SetClearColor(0.18f, 0.18f, 0.20f, 1.0f);

		m_Shader = m_RenderResources.CreateShaderFromFile(
			String("Basic"),
			String("shaders/basic.vert"),
			String("shaders/basic.frag")
		);
	}

	Application::~Application()
	{
		if (m_ImGuiLayer && m_ImGuiFactory)
			m_ImGuiFactory->Delete(m_ImGuiLayer);

		if (m_WindowHandler && m_WindowHandlerFactory)
			m_WindowHandlerFactory->Delete(m_WindowHandler);

		m_RenderResources.Shutdown();
	}

	void Application::PushLayer(Layer* layer)
	{
		m_LayerStack.PushLayer(layer);
	}

	void Application::PushOverlay(Layer* overlay)
	{
		m_LayerStack.PushLayer(overlay);
	}

	void Application::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowCloseEvent>(BIND_EVENT_FN(OnWindowClosed));
		dispatcher.Dispatch<WindowResizeEvent>(BIND_EVENT_FN(OnWindowResized));

		CHE_CORE_TRACE("{0}", e);

		for (auto it = m_LayerStack.end(); it != m_LayerStack.begin();)
		{
			(*--it)->OnEvent(e);
			if (e.Handled)
				break;
		}
	}

	bool Application::OnWindowClosed(WindowCloseEvent& e)
	{
		m_Running = false;
		return true;
	}

	bool Application::OnWindowResized(WindowResizeEvent& e)
	{
		m_RenderResources.Get(m_RenderApi)->SetViewport(e.GetWidth(), e.GetHeight());
		return false;
	}

	void Application::Run()
	{
		while (m_Running)
		{
			m_RenderResources.Get(m_RenderApi)->Clear();

			// Layers handle their own rendering and logic
			for (Layer* layer : m_LayerStack)
				layer->OnUpdate();

			if (m_ImGuiLayer)
			{
				m_ImGuiLayer->Begin();
				for (Layer* layer : m_LayerStack)
					layer->OnImGuiRender();
				m_ImGuiLayer->End();
			}

			if (m_WindowHandler)
			{
				m_WindowHandler->SwapBuffers();
				m_WindowHandler->PollEvents();
			}
		}
	}
}
