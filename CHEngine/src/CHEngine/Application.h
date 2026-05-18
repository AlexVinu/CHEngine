#pragma once

#include <Core.h>

#include "Events/Event.h"
#include "CHEngine/Events/ApplicationEvent.h"
#include "CHEngine/Layer/LayerStack.h"
#include "Window.h"

#include "WindowSystem/IWindowFactory.h"
#include "Physics/IPhysicsFactory.h"
#include "Render/IRenderFactory.h"

#include "Render/RenderSubsystem.h"
#include "Physics/PhysicsSubsystem.h"
#include "UI/UISubsystem.h"
#include "UI/UIRendererBackend.h"
#include "ResourceManager/ResourceManager.h"

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
        bool        PhysicsEnabled          = true;
    };

    class CHENGINE_API Application
    {
    public:
        Application(const ApplicationConfig& config = {});
        virtual ~Application();

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

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

        ERenderAPI GetRenderAPIType() const { return m_RenderAPIType; }

        // ── Subsystem accessors ───────────────────────────────────────────────
        // Render is always valid after a successful startup.
        RenderSubsystem&  Render()    { CHE_CORE_ASSERT(m_Render,   "Render subsystem not initialised"); return *m_Render; }

        // Physics and UI are optional — check HasPhysics()/HasUI() before calling.
        PhysicsSubsystem*  Physics()   { return m_Physics.get();   }
        UISubsystem*       UI()        { return m_UI.get();        }
        UIRendererBackend* NativeUI()  { return m_NativeUI.get();  }
        ResourceManager&   Resources() { return *m_Resources;      }

        bool HasPhysics()  const { return m_Physics  != nullptr; }
        bool HasUI()       const { return m_UI       != nullptr; }
        bool HasNativeUI() const { return m_NativeUI != nullptr; }

        // Legacy: direct factory pointer (used by renderer modules themselves)
        IRenderFactory* GetRenderFactory() const { return m_Render ? m_Render->GetRenderFactory() : nullptr; }

        void RequestRestart();
        bool IsRestartRequested() const { return m_RestartRequested; }

        // Legacy active-shader helpers (used by Application::Run and layers).
        ShaderHandle GetActiveShader() const         { return m_Shader; }
        void         SetActiveShader(ShaderHandle h) { m_Shader = h; }

    private:
        friend Layer;

        bool OnWindowClosed(WindowCloseEvent& e);
        bool OnWindowResized(WindowResizeEvent& e);

        static Application* s_Instance;

        // ── Modules (FIRST — must outlive all subsystems) ─────────────────────
        Scope<ModuleManager> m_ModuleManager;

        // ── Window ────────────────────────────────────────────────────────────
        Scope<Window> m_Window;

        // ── Subsystems (destruction order = reverse of declaration order) ─────
        // Render owns IRenderFactory* → must outlive Window.
        // ResourceManager holds handles into Render → declared AFTER m_Render,
        // destructs BEFORE it (C++ reverses declaration order).
        // Physics and UI are optional (nullptr when disabled/unavailable).
        // All Scope<> members destruct BEFORE m_ModuleManager (which unloads DLLs).
        Scope<RenderSubsystem>   m_Render;
        Scope<PhysicsSubsystem>  m_Physics;
        Scope<UISubsystem>       m_UI;
        Scope<UIRendererBackend> m_NativeUI;
        Scope<ResourceManager>   m_Resources;

        // ── Factory pointers (non-owning, lifetime = module) ──────────────────
        IWindowFactory*  m_WindowFactory  = nullptr;
        IImGuiFactory*   m_ImGuiFactory   = nullptr;
        IPhysicsFactory* m_PhysicsFactory = nullptr;

        bool       m_Running = true;
        LayerStack m_LayerStack;

        ShaderHandle m_Shader;

        ERenderAPI m_RenderAPIType    = ERenderAPI::OPENGL;
        bool       m_RestartRequested = false;

        std::chrono::steady_clock::time_point m_LastFrameTime;
    };

    // To be defined in client
    Application* CreateApplication(const ApplicationConfig& config);

}
