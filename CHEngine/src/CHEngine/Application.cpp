#include "chepch.h"
#include "Application.h"

#include "Log/Log.h"

namespace CHEngine {

#define BIND_EVENT_FN(x) std::bind(&Application::x, this, std::placeholders::_1)

	Application* Application::s_Instance = nullptr;

	Application::Application()
		:m_ModuleManager()
	{
		CHE_CORE_ASSERT(!s_Instance, "Application already exists!");
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

		m_VertexArray = m_RenderResources.CreateVertexArray();

		float vertices[3 * 3] = {
			-0.5f, -0.5f, 0.0f,
			 0.5f, -0.5f, 0.0f,
			 0.0f,  0.5f, 0.0f
		};

		auto vertexBuffer = m_RenderResources.CreateVertexBuffer(vertices, sizeof(vertices));
		BufferLayout layout = {
			{ ShaderDataType::Float3, "a_Position" },
		};
		vertexBuffer->SetLayout(layout);
		m_RenderResources.Get(m_VertexArray)->AddVertexBuffer(vertexBuffer);

		uint32_t indices[3] = { 0, 1, 2 };
		auto indexBuffer = m_RenderResources.CreateIndexBuffer(indices, sizeof(indices) / sizeof(uint32_t));

		m_RenderResources.Get(m_VertexArray)->SetIndexBuffer(indexBuffer);

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

			// Bind active shader first so that OnUpdate() can set uniforms
			m_RenderResources.Get(m_Shader)->Bind();

			// Layers: game logic + uniform setup
			for (Layer* layer : m_LayerStack)
				layer->OnUpdate();

			// Draw with whatever uniforms the layers have set
			m_RenderResources.Get(m_RenderApi)->DrawIndexed(m_RenderResources.Get(m_VertexArray));

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
