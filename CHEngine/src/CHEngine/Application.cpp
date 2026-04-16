#include "chepch.h"
#include "Application.h"

#include "Log/Log.h"
#include "PlatformAPICapabilities.h"
#include "Profiler.h"
#include "CHEngine/Input/Input.h"
#include "CHEngine/EngineConfig.h"
#include "Physics/PhysicsFacade.h"
#include "Render/RenderFacade.h"
#include "UI/UIFacade.h"

#include <imgui.h>

#include <filesystem>
#if defined(CHE_PLATFORM_APPLE)
#include <mach-o/dyld.h>
#elif defined(CHE_PLATFORM_WINDOWS)
#include <Windows.h>
#elif defined(CHE_PLATFORM_LINUX)
#include <unistd.h>
#include <climits>      // PATH_MAX
#endif

namespace CHEngine {

    namespace
    {
        struct StartupModuleSelection
        {
            std::string WindowModuleName;
            std::string RendererModuleName;
            std::string ImGuiModuleName;
            std::string PhysicsModuleName;
        };

        const char* GetDefaultWindowModuleName()
        {
#if defined(CHE_PLATFORM_WINDOWS)
            return "WindowGLFW.dll";
#elif defined(CHE_PLATFORM_APPLE)
            return "libWindowGLFW.dylib";
#else
            return "libWindowGLFW.so";
#endif
        }

        const char* GetDefaultPhysicsModuleName()
        {
#if defined(CHE_PLATFORM_WINDOWS)
            return "PhysicsPhysX.dll";
#elif defined(CHE_PLATFORM_APPLE)
            return "libPhysicsPhysX.dylib";
#else
            return "libPhysicsPhysX.so";
#endif
        }

        bool TryLoadStartupModules(ModuleManager* module_manager,
                                   const ApplicationConfig& config,
                                   ERenderAPI render_api,
                                   StartupModuleSelection* out_selection,
                                   IWindowFactory** out_window_factory,
                                   IRenderFactory** out_render_factory,
                                   IImGuiFactory** out_imgui_factory,
                                   IPhysicsFactory** out_physics_factory)
        {
            if (!module_manager || !out_selection || !out_window_factory || !out_render_factory
                || !out_imgui_factory || !out_physics_factory)
                return false;

            const ModuleNames module_names = RenderModuleResolver::GetModuleNames(render_api);
            if (!module_names.Renderer || !module_names.ImGui)
            {
                CHE_CORE_WARN("Render API {} is not supported by platform module mapping", (int)render_api);
                return false;
            }

            out_selection->WindowModuleName = config.WindowModuleOverride ? config.WindowModuleOverride : GetDefaultWindowModuleName();
            out_selection->RendererModuleName = config.RendererModuleOverride ? config.RendererModuleOverride : module_names.Renderer;
            out_selection->ImGuiModuleName = config.ImGuiModuleOverride ? config.ImGuiModuleOverride : module_names.ImGui;
            out_selection->PhysicsModuleName = config.PhysicsModuleOverride ? config.PhysicsModuleOverride : GetDefaultPhysicsModuleName();

            if (config.WindowModuleOverride)
                CHE_CORE_INFO("Window module override: {}", out_selection->WindowModuleName.c_str());
            if (config.RendererModuleOverride)
                CHE_CORE_INFO("Renderer module override: {}", out_selection->RendererModuleName.c_str());
            if (config.ImGuiModuleOverride)
                CHE_CORE_INFO("ImGui module override: {}", out_selection->ImGuiModuleName.c_str());
            if (config.PhysicsModuleOverride)
                CHE_CORE_INFO("Physics module override: {}", out_selection->PhysicsModuleName.c_str());

            const bool window_loaded = module_manager->LoadModule(out_selection->WindowModuleName);
            const bool renderer_loaded = module_manager->LoadModule(out_selection->RendererModuleName);
            const bool imgui_loaded = module_manager->LoadModule(out_selection->ImGuiModuleName);

            bool physics_loaded = false;
            if (config.PhysicsEnabled)
                physics_loaded = module_manager->LoadModule(out_selection->PhysicsModuleName);
            else
                CHE_CORE_INFO("Physics module loading disabled by startup config.");

            *out_window_factory = module_manager->GetModule<IWindowFactory>(ModuleType::Window);
            *out_render_factory = module_manager->GetModule<IRenderFactory>(ModuleType::Render);
            *out_imgui_factory = module_manager->GetModule<IImGuiFactory>(ModuleType::ImGui);
            *out_physics_factory = physics_loaded ? module_manager->GetModule<IPhysicsFactory>(ModuleType::Physics) : nullptr;

            if (!window_loaded || !*out_window_factory)
            {
                CHE_CORE_ERROR("Failed to load Window module '{}'", out_selection->WindowModuleName.c_str());
                module_manager->UnloadAll();
                return false;
            }
            if (!renderer_loaded || !*out_render_factory)
            {
                CHE_CORE_ERROR("Failed to load Renderer module '{}'", out_selection->RendererModuleName.c_str());
                module_manager->UnloadAll();
                return false;
            }
            CHE_CORE_INFO("Running renderer health-check: {}", out_selection->RendererModuleName.c_str());
            if (!(*out_render_factory)->CheckIsWorking())
            {
                CHE_CORE_ERROR("Renderer module '{}' failed CheckIsWorking()", out_selection->RendererModuleName.c_str());
                module_manager->UnloadAll();
                return false;
            }
            if (!imgui_loaded || !*out_imgui_factory)
                CHE_CORE_WARN("Failed to load ImGui module '{}'! ImGui will be unavailable.", out_selection->ImGuiModuleName.c_str());

            if (config.PhysicsEnabled && *out_physics_factory)
            {
                CHE_CORE_INFO("Running physics health-check: {}", out_selection->PhysicsModuleName.c_str());
                if (!(*out_physics_factory)->CheckIsWorking())
                {
                    CHE_CORE_WARN("Physics module '{}' failed CheckIsWorking(); physics will be disabled.",
                                  out_selection->PhysicsModuleName.c_str());
                    *out_physics_factory = nullptr;
                }
            }
            else if (config.PhysicsEnabled)
            {
                CHE_CORE_WARN("Failed to load Physics module '{}'! Physics will be unavailable.",
                              out_selection->PhysicsModuleName.c_str());
            }

            return true;
        }
    }

    Application* Application::s_Instance = nullptr;

    Application::Application(const ApplicationConfig& config)
        : m_RenderAPIType(config.RenderAPI)
    {
        m_ModuleManager = std::make_unique<ModuleManager>();
        m_RenderResources = std::make_unique<RenderResourceManager>();
        CHE_CORE_ASSERT(!s_Instance, "Application already exists!");

        // Set working directory to executable location so relative paths
        // (shaders/, assets/) resolve correctly on all platforms.
        {
            std::filesystem::path exeDir;
#if defined(CHE_PLATFORM_APPLE)
            char exePath[4096];
            uint32_t size = sizeof(exePath);
            if (_NSGetExecutablePath(exePath, &size) == 0)
                exeDir = std::filesystem::path(exePath).parent_path();
#elif defined(CHE_PLATFORM_WINDOWS)
            char exePath[MAX_PATH];
            if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) > 0)
                exeDir = std::filesystem::path(exePath).parent_path();
#elif defined(CHE_PLATFORM_LINUX)
            char exePath[PATH_MAX];
            ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
            if (len > 0)
            {
                exePath[len] = '\0';
                exeDir = std::filesystem::path(exePath).parent_path();
            }
#endif
            if (!exeDir.empty())
            {
                std::filesystem::current_path(exeDir);
                CHE_CORE_INFO("Working directory set to: {0}", exeDir.string().c_str());
            }
        }
        s_Instance = this;

        StartupModuleSelection startup_selection;
        IRenderFactory* render_factory = nullptr;
        ERenderAPI selected_api = config.RenderAPI;
        bool startup_ok = TryLoadStartupModules(m_ModuleManager.get(),
                                                config,
                                                selected_api,
                                                &startup_selection,
                                                &m_WindowFactory,
                                                &render_factory,
                                                &m_ImGuiFactory,
                                                &m_PhysicsFactory);

        const bool has_render_override = config.RendererModuleOverride || config.ImGuiModuleOverride;
        if (!startup_ok && !has_render_override && selected_api != ERenderAPI::OPENGL)
        {
            CHE_CORE_WARN("Startup failed for render API {}. Falling back to OpenGL.", (int)selected_api);
            selected_api = ERenderAPI::OPENGL;
            startup_ok = TryLoadStartupModules(m_ModuleManager.get(),
                                               config,
                                               selected_api,
                                               &startup_selection,
                                               &m_WindowFactory,
                                               &render_factory,
                                               &m_ImGuiFactory,
                                               &m_PhysicsFactory);
        }

        if (!startup_ok)
        {
            CHE_CORE_CRITICAL("Failed to initialize startup modules.");
            m_Running = false;
            return;
        }

        m_RenderAPIType = selected_api;

        // Все модули загружены успешно — фиксируем рендерер как рабочий.
        // Если движок до этой точки крашнулся, renderer_pending уже очищен
        // в LoadRendererPreference, и следующий запуск вернётся к прежнему renderer.
        EngineConfig::CommitRendererPreference(m_RenderAPIType);

        m_RenderResources->Init(render_factory);

        // ─── 2. Создать окно ────────────────────────────────────────────────
        ERenderAPI render_api = render_factory->GetRenderApi();
        m_Window = Scope<Window>(Window::Create(m_WindowFactory, render_api));
        m_Window->SetEventCallback([this](Event& e) { OnEvent(e); });

        // ─── 3. Инициализировать IRenderApi (GLAD / device / Vulkan), затем IRenderer (только draw) ───
        RendererInitInfo renderInitInfo = m_Window->GetPlatformWindow()->GetRenderInitInfo(render_api);
        RenderFacade::InitRenderer(renderInitInfo);

        // ─── 4. Создать ImGui layer ──────────────────────────────────────────
        // ImGuiOGL линкован с тем же shared libglfw.dylib → видит то же окно
        if (m_ImGuiFactory)
            UIFacade::SetLayer(m_ImGuiFactory->CreateImGuiLayer(m_Window->GetPlatformWindow()));

        // ─── 5. Создать рендер-объекты (нужен GLAD / API Init, поэтому после шага 3) ───
        RenderFacade::SetClearColor(0.18f, 0.18f, 0.20f, 1.0f);

        m_Shader = m_RenderResources->CreateShaderFromFile(
            String("Basic"),
            String("shaders/basic.vert"),
            String("shaders/basic.frag")
        );

        if (m_PhysicsFactory)
        {
            if (!PhysicsFacade::Init(m_PhysicsFactory))
                CHE_CORE_WARN("Failed to initialize physics facade!");
        }

    }

    Application::~Application()
    {
        if (IImGuiLayer* layer = UIFacade::GetLayer(); layer && m_ImGuiFactory)
            m_ImGuiFactory->Delete(layer);
        UIFacade::SetLayer(nullptr);

        PhysicsFacade::Shutdown();

        m_RenderResources->Shutdown();
    }

    void Application::PushLayer(Layer* layer)
    {
        m_LayerStack.PushLayer(layer);
    }

    void Application::PushOverlay(Layer* overlay)
    {
        m_LayerStack.PushOverlay(overlay);
    }

    void Application::OnEvent(Event& e)
    {
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<WindowCloseEvent>([this](WindowCloseEvent& event) { return OnWindowClosed(event); });
        dispatcher.Dispatch<WindowResizeEvent>([this](WindowResizeEvent& event) { return OnWindowResized(event); });

        for (auto it = m_LayerStack.end(); it != m_LayerStack.begin();)
        {
            (*--it)->OnEvent(e);
            if (e.Handled)
                break;
        }
    }

    bool Application::OnWindowClosed(WindowCloseEvent& /*e*/)
    {
        m_Running = false;
        return true;
    }

    bool Application::OnWindowResized(WindowResizeEvent& e)
    {
        RenderFacade::SetViewport(e.GetWidth(), e.GetHeight());
        return false;
    }

    void Application::RequestRestart()
    {
        m_RestartRequested = true;
        m_Running = false;
    }

    void Application::Run()
    {
        m_LastFrameTime      = std::chrono::steady_clock::now();
        float shaderPollAcc  = 0.0f;

        constexpr float ShaderPollInterval = 0.5f; // шейдеры — раз в полсекунды

        while (m_Running)
        {
            // ── Delta time ──────────────────────────────────────────────────
            auto now = std::chrono::steady_clock::now();
            Timestep dt = std::chrono::duration<float>(now - m_LastFrameTime).count();
            m_LastFrameTime = now;

            Profiler::Flush();

            // ── Shader hot reload ────────────────────────────────────────────
            shaderPollAcc += dt;
            if (shaderPollAcc >= ShaderPollInterval) {
                shaderPollAcc = 0.0f;
                m_RenderResources->PollShaders();
            }

            // ── Input: опросить состояние клавиатуры/мыши БЕЗ OS-задержки ──
            Input::BeginFrame(m_Window->GetPlatformWindow());

            // ── BeginFrame (Metal: drawable + command buffer + encoder) ──
            if (m_RenderResources->GetRenderAPI())
                RenderFacade::BeginFrame();

            RenderFacade::Clear();

            for (Layer* layer : m_LayerStack)
                layer->OnUpdate(dt);

            if (UIFacade::GetLayer())
            {
                // Передать нативный контекст (Metal нужны Device/CmdBuf/Encoder)
                if (IRenderApi* api = m_RenderResources->GetRenderAPI())
                    UIFacade::SetRenderContext(api->GetRenderContext());

                UIFacade::Begin();
                for (Layer* layer : m_LayerStack)
                    layer->OnImGuiRender();
                UIFacade::End();
            }

            // ── EndFrame (Metal: end encoding, present, commit) ──
            if (m_RenderResources->GetRenderAPI())
                RenderFacade::EndFrame();

            m_Window->OnUpdate();
        }

        // ── Overlay «Restarting...» — один финальный кадр ────────────────────
        if (m_RestartRequested && UIFacade::GetLayer())
        {
            if (m_RenderResources->GetRenderAPI())
                RenderFacade::BeginFrame();

            RenderFacade::Clear();

            RenderFacade::BeginScene();
            RenderFacade::EndScene();

            if (IRenderApi* api = m_RenderResources->GetRenderAPI())
                UIFacade::SetRenderContext(api->GetRenderContext());

            UIFacade::Begin();

            ImVec2 displaySize = ImGui::GetIO().DisplaySize;
            ImGui::GetBackgroundDrawList()->AddRectFilled(
                ImVec2(0, 0), displaySize, IM_COL32(0, 0, 0, 180));

            const char* text = "Restarting with new renderer...";
            ImVec2 center(displaySize.x * 0.5f, displaySize.y * 0.5f);

            ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(0, 0));
            ImGui::Begin("##restart_overlay", nullptr,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground |
                ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::TextUnformatted(text);
            ImGui::End();

            UIFacade::End();

            if (m_RenderResources->GetRenderAPI())
                RenderFacade::EndFrame();

            m_Window->OnUpdate();
        }
    }
}
