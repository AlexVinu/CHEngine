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

#define BIND_EVENT_FN(x) std::bind(&Application::x, this, std::placeholders::_1)

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

        // Проверка работоспособности всех доступных системе рендеров
        for (auto api : RenderAPICaps::AllAvailableAPI())
        {
            auto module_name = RenderAPICaps::GetModuleNames(api);
            bool flag = module_name.Renderer && (m_ModuleManager->LoadModule(module_name.Renderer) &&
                m_ModuleManager->GetModule<IRenderFactory>(ModuleType::Render)->CheckIsWorking());
            
            RenderAPICaps::SetFlag(api, flag);
        }

        m_ModuleManager->UnloadAll();

        CHE_CORE_INFO("ALL AVAILABLE RENDER API ------------------");
        for (auto api : RenderAPICaps::AllAvailableAPI())
        {
            CHE_CORE_INFO("RENDER API {}", api.c_str());
        }

        // ─── 1. Загрузить все 3 модуля ───────────────────────────────────────
        // Порядок важен: Window → Renderer → ImGui
        // Имена модулей определяются выбранным ERenderAPI

        auto moduleNames = RenderAPICaps::GetModuleNames(m_RenderAPIType);
        if (!moduleNames.Renderer) {
            CHE_CORE_CRITICAL("ERenderAPI {} is not supported on this platform!", (int)m_RenderAPIType);
            m_Running = false;
            return;
        }

#if defined(CHE_PLATFORM_WINDOWS)
        m_ModuleManager->LoadModule("WindowGLFW.dll");
#elif defined(CHE_PLATFORM_APPLE)
        m_ModuleManager->LoadModule("libWindowGLFW.dylib");
#else
        m_ModuleManager->LoadModule("libWindowGLFW.so");
#endif
        m_ModuleManager->LoadModule(moduleNames.Renderer);
        m_ModuleManager->LoadModule(moduleNames.ImGui);

        bool physicsLoaded = false;
#if defined(CHE_PLATFORM_WINDOWS)
        physicsLoaded = m_ModuleManager->LoadModule("PhysicsPhysX.dll");
#elif defined(CHE_PLATFORM_APPLE)
        physicsLoaded = m_ModuleManager->LoadModule("libPhysicsPhysX.dylib");
#else
        physicsLoaded = m_ModuleManager->LoadModule("libPhysicsPhysX.so");
#endif

        m_WindowFactory = m_ModuleManager->GetModule<IWindowFactory>(ModuleType::Window);
        auto render_factory = m_ModuleManager->GetModule<IRenderFactory>(ModuleType::Render);
        m_ImGuiFactory  = m_ModuleManager->GetModule<IImGuiFactory>(ModuleType::ImGui);
        if (physicsLoaded)
            m_PhysicsFactory = m_ModuleManager->GetModule<IPhysicsFactory>(ModuleType::Physics);

        if (!m_WindowFactory)
        {
            CHE_CORE_CRITICAL("Failed to load Window module! Cannot continue.");
            m_Running = false;
            return;
        }
        if (!render_factory)
        {
            CHE_CORE_CRITICAL("Failed to load Renderer module! Cannot continue.");
            m_Running = false;
            return;
        }
        if (!m_ImGuiFactory)
            CHE_CORE_ERROR("Failed to load ImGuiOGL module! ImGui will be unavailable.");
        if (!m_PhysicsFactory)
            CHE_CORE_WARN("Failed to load Physics module! Physics will be unavailable.");

        // Все модули загружены успешно — фиксируем рендерер как рабочий.
        // Если движок до этой точки крашнулся, renderer_pending уже очищен
        // в LoadRendererPreference, и следующий запуск вернётся к прежнему renderer.
        EngineConfig::CommitRendererPreference(m_RenderAPIType);

        m_RenderResources->Init(render_factory);

        // ─── 2. Создать окно ────────────────────────────────────────────────
        ERenderAPI render_api = render_factory->GetRenderApi();
        m_Window = Scope<Window>(Window::Create(m_WindowFactory, render_api));
        m_Window->SetEventCallback(BIND_EVENT_FN(OnEvent));

        // ─── 3. Инициализировать IRenderApi (GLAD / device / Vulkan), затем IRenderer (только draw) ───
        RendererInitInfo renderInitInfo = m_Window->GetPlatformWindow()->GetRenderInitInfo(render_api);
        RenderFacade::InitRenderer(renderInitInfo);

        // ─── 4. Создать ImGui layer ──────────────────────────────────────────
        // ImGuiOGL линкован с тем же shared libglfw.dylib → видит то же окно
        if (m_ImGuiFactory)
            UIFacade::SetLayer(m_ImGuiFactory->CreateImGuiLayer(m_Window->GetPlatformWindow()));

        // ─── 4а. Зарегистрировать hot reload для ImGuiOGL ────────────────────
        // WindowGLFW и RendererOGL не перезагружаем — они держат GL-контекст
        if (m_ImGuiFactory) {
            m_ModuleManager->Watch(ModuleType::ImGui, {
                // Уничтожить ImGui-слой до выгрузки dylib
                [this]() {
                    if (IImGuiLayer* layer = UIFacade::GetLayer(); layer && m_ImGuiFactory)
                        m_ImGuiFactory->Delete(layer);
                    UIFacade::SetLayer(nullptr);
                    m_ImGuiFactory = nullptr;
                },
                // Пересоздать после загрузки нового dylib
                [this](IModuleFactory* factory) {
                    m_ImGuiFactory = static_cast<IImGuiFactory*>(factory);
                    if (m_Window)
                        UIFacade::SetLayer(m_ImGuiFactory->CreateImGuiLayer(m_Window->GetPlatformWindow()));
                }
            });
        }

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

        m_RuntimeWorlds.emplace_back();
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
        dispatcher.Dispatch<WindowCloseEvent>(BIND_EVENT_FN(OnWindowClosed));
        dispatcher.Dispatch<WindowResizeEvent>(BIND_EVENT_FN(OnWindowResized));

        if (!e.Handled)
        {
            for (World& world : m_RuntimeWorlds)
            {
                world.OnEvent(e);
                if (e.Handled)
                    break;
            }
        }

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
        float modulePollAcc  = 0.0f;

        constexpr float ShaderPollInterval = 0.5f; // шейдеры — раз в полсекунды
        constexpr float ModulePollInterval = 1.0f; // модули  — раз в секунду

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

            // ── Module hot reload ────────────────────────────────────────────
            modulePollAcc += dt;
            if (modulePollAcc >= ModulePollInterval) {
                modulePollAcc = 0.0f;
                m_ModuleManager->Poll();
            }

            // ── Input: опросить состояние клавиатуры/мыши БЕЗ OS-задержки ──
            Input::BeginFrame(m_Window->GetPlatformWindow());

            // ── BeginFrame (Metal: drawable + command buffer + encoder) ──
            if (m_RenderResources->GetRenderAPI())
                RenderFacade::BeginFrame();

            RenderFacade::Clear();

            for (Layer* layer : m_LayerStack)
                layer->OnUpdate(dt);

            for (World& world : m_RuntimeWorlds)
                world.update(dt);

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
