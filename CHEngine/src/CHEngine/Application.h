#pragma once

#include <Core.h>

#include "Events/Event.h"
#include "CHEngine/Events/ApplicationEvent.h"
#include "CHEngine/Layer/LayerStack.h"
#include "Window.h"

#include "Render/RenderResourceManager.h"
#include "Render/IImGuiLayer.h"

#include "IWindowFactory.h"
#include "IImGuiFactory.h"

#include "ModuleManager.h"

#include <chrono>

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

        static Application& Get()
        {
            CHE_CORE_ASSERT(s_Instance, "Application not created yet!");
            return *s_Instance;
        }

        RenderResourceManager& GetRenderResources() { return m_RenderResources; }
        RenderAPIHandle GetRenderApiHandle() const { return m_RenderApi; }

        ShaderHandle GetActiveShader() const         { return m_Shader; }
        void         SetActiveShader(ShaderHandle h)  { m_Shader = h; }

    private:
        bool OnWindowClosed(WindowCloseEvent& e);
        bool OnWindowResized(WindowResizeEvent& e);

        static Application* s_Instance;

        ModuleManager         m_ModuleManager;
        RenderResourceManager m_RenderResources;

        IWindowFactory* m_WindowFactory = nullptr;
        IRenderFactory* m_RenderFactory = nullptr;
        IImGuiFactory*  m_ImGuiFactory  = nullptr;

        bool            m_Running = true;
        LayerStack      m_LayerStack;

        ShaderHandle    m_Shader;
        RenderAPIHandle m_RenderApi;

        IImGuiLayer* m_ImGuiLayer  = nullptr;
        IRenderer*   m_OGLRenderer = nullptr;  // держит GLAD инициализированным

        std::unique_ptr<Window> m_Window;

        // Delta time
        std::chrono::steady_clock::time_point m_LastFrameTime;
    };

    // To be defined in client
    Application* CreateApplication();

}
