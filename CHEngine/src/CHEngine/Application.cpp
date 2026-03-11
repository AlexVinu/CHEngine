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
		#elif defined(CHE_PLATFORM_APPLE)
		m_ModuleManager.LoadModule("libRendererOGL.dylib");
		#else
		m_ModuleManager.LoadModule("libRendererOGL.so");
		#endif
		m_RenderFactory = m_ModuleManager.GetModule<IRenderFactory>(ModuleType::Render);

		m_RenderResources.Init(m_RenderFactory);

		m_Window = std::unique_ptr<Window>(Window::Create(m_RenderFactory));
		m_Window->SetEventCallback(BIND_EVENT_FN(OnEvent));

		m_ImGuiLayer = m_RenderFactory->CreateImGuiLayer(m_Window->GetNativeWindow());

		m_RenderApi = m_RenderResources.CreateRenderAPI();
		m_RenderResources.Get(m_RenderApi)->SetClearColor(0.12f, 0.12f, 0.12f, 1.0f);

		m_Shader = m_RenderResources.CreateShaderFromFile(
			String("Basic"),
			String("shaders/basic.vert"),
			String("shaders/basic.frag")
		);
	}

	Application::~Application()
	{
		if (m_ImGuiLayer && m_RenderFactory)
			m_RenderFactory->Delete(m_ImGuiLayer);

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

			m_Window->OnUpdate();
		}
	}
}
