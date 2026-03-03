#pragma once

#include <Core.h>

#include "Events/Event.h"
#include "CHEngine/Events/ApplicationEvent.h"
#include "CHEngine/Layer/LayerStack.h"
#include "Window.h"

#include "Render/RenderResourceManager.h"

#include "ModuleManager.h"

namespace CHEngine {

	class CHENGINE_API Application
	{
	public:
		Application();
		virtual ~Application();

		void Run();

		void OnEvent(Event& e);

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* overlay);

	private:
		bool OnWindowClosed(WindowCloseEvent& e);
		bool OnWindowResized(WindowResizeEvent& e);

		ModuleManager m_ModuleManager;
		RenderResourceManager m_RenderResources;

		bool m_Running = true;
		LayerStack m_LayerStack;

		ShaderHandle      m_Shader;
		VertexArrayHandle m_VertexArray;
		RenderAPIHandle   m_RenderApi;

		std::unique_ptr<Window> m_Window;
	};

	// To be defined in client
	Application* CreateApplication();

}
