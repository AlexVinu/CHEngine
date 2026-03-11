#pragma once

#include <Core.h>

#include "Events/Event.h"
#include "CHEngine/Events/ApplicationEvent.h"
#include "CHEngine/Layer/LayerStack.h"
#include "Window.h"

#include "Render/RenderResourceManager.h"
#include "Render/IImGuiLayer.h"

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

		// Global singleton access (set in constructor).
		static Application& Get() { return *s_Instance; }

		RenderResourceManager& GetRenderResources() { return m_RenderResources; }
		RenderAPIHandle GetRenderApiHandle() const { return m_RenderApi; }

		ShaderHandle GetActiveShader() const        { return m_Shader; }
		void         SetActiveShader(ShaderHandle h) { m_Shader = h; }

	private:
		bool OnWindowClosed(WindowCloseEvent& e);
		bool OnWindowResized(WindowResizeEvent& e);

		static Application* s_Instance;

		ModuleManager m_ModuleManager;
		RenderResourceManager m_RenderResources;
		IRenderFactory* m_RenderFactory = nullptr;

		bool m_Running = true;
		LayerStack m_LayerStack;

		ShaderHandle      m_Shader;
		RenderAPIHandle   m_RenderApi;

		IImGuiLayer* m_ImGuiLayer = nullptr;

		std::unique_ptr<Window> m_Window;
	};

	// To be defined in client
	Application* CreateApplication();

}
