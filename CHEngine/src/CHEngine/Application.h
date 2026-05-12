#pragma once

#include <Core.h>

#include "Events/Event.h"
#include "CHEngine/Events/ApplicationEvent.h"
#include "CHEngine/Layer/LayerStack.h"
#include "Window.h"

#include "UI/UIFacade.h"

#include "WindowSystem/IWindowFactory.h"
#include "Physics/IPhysicsFactory.h"
#include "Render/IRenderFactory.h"

#include "ModuleManager.h"

#include "RenderData.h"

#include "Timestep.h"
#include "World/World.h"
#include <chrono>

namespace CHEngine {

    struct CHENGINE_API ApplicationConfig
    {
        ERenderAPI  RenderAPI = ERenderAPI::OPENGL;
        const char* WindowModuleOverride    = nullptr;
        const char* RendererModuleOverride  = nullptr;
        const char* ImGuiModuleOverride     = nullptr;
        const char* PhysicsModuleOverride   = nullptr;
        bool        PhysicsEnabled          =
#if defined(CHE_PLATFORM_APPLE)
            false;  // PhysX is not available on macOS
#else
            true;
#endif
    };

    class CHENGINE_API Application
    {
    public:
        Application(const ApplicationConfig& config = {});
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

        Window* GetWindow() const { return m_Window.get(); }

        ERenderAPI     GetRenderAPIType()   const { return m_RenderAPIType; }
        IRenderFactory* GetRenderFactory()  const { return m_RenderFactory; }

        void RequestRestart();
        bool IsRestartRequested() const { return m_RestartRequested; }

        ShaderHandle GetActiveShader() const         { return m_Shader; }
        void         SetActiveShader(ShaderHandle h) { m_Shader = h; }

    private:
        bool OnWindowClosed(WindowCloseEvent& e);
        bool OnWindowResized(WindowResizeEvent& e);

        static Application* s_Instance;

        Scope<ModuleManager>  m_ModuleManager;

        IWindowFactory*  m_WindowFactory  = nullptr;
        IImGuiFactory*   m_ImGuiFactory   = nullptr;
        IPhysicsFactory* m_PhysicsFactory = nullptr;
        IRenderFactory*  m_RenderFactory  = nullptr;

        bool       m_Running = true;
        LayerStack m_LayerStack;

        ShaderHandle m_Shader;

        ERenderAPI m_RenderAPIType       = ERenderAPI::OPENGL;
        bool       m_RestartRequested    = false;

        Scope<Window> m_Window;

        std::chrono::steady_clock::time_point m_LastFrameTime;
    };

    // To be defined in client
    Application* CreateApplication(const ApplicationConfig& config);

}
