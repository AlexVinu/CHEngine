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
        : m_ModuleManager()
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

        // ─── 1. Загрузить все 3 модуля ───────────────────────────────────────
        // Порядок важен: Window → Renderer → ImGui
        // libglfw.dylib и libglad.dylib — shared, загружаются ОС один раз
        // и видны всем трём модулям (это решает проблему с контекстом GLFW/ImGui)

#if defined(CHE_PLATFORM_WINDOWS)
        m_ModuleManager.LoadModule("WindowGLFW.dll");
        m_ModuleManager.LoadModule("RendererOGL.dll");
        m_ModuleManager.LoadModule("ImGuiOGL.dll");
#elif defined(CHE_PLATFORM_APPLE)
        m_ModuleManager.LoadModule("libWindowGLFW.dylib");
        m_ModuleManager.LoadModule("libRendererOGL.dylib");
        m_ModuleManager.LoadModule("libImGuiOGL.dylib");
#else
        m_ModuleManager.LoadModule("libWindowGLFW.so");
        m_ModuleManager.LoadModule("libRendererOGL.so");
        m_ModuleManager.LoadModule("libImGuiOGL.so");
#endif

        m_WindowFactory = m_ModuleManager.GetModule<IWindowFactory>(ModuleType::Window);
        m_RenderFactory = m_ModuleManager.GetModule<IRenderFactory>(ModuleType::Render);
        m_ImGuiFactory  = m_ModuleManager.GetModule<IImGuiFactory>(ModuleType::ImGui);

        if (!m_ImGuiFactory)
            CHE_CORE_ERROR("Failed to load ImGuiOGL module! ImGui will be unavailable.");

        m_RenderResources.Init(m_RenderFactory);

        // ─── 2. Создать окно (GLFW window + context, glfwMakeContextCurrent) ─
        ERenderAPI render_api = m_RenderFactory->GetRenderApi();
        m_Window = std::unique_ptr<Window>(Window::Create(m_WindowFactory, m_RenderFactory->GetRenderApi()));
        m_Window->SetEventCallback(BIND_EVENT_FN(OnEvent));

        // ─── 3. Инициализировать OpenGL (GLAD) с уже существующим контекстом ─
        m_OGLRenderer = m_RenderFactory->CreateRenderer(m_Window->GetPlatformWindow()->GetRenderInitInfo(render_api));

        // ─── 4. Создать ImGui layer ──────────────────────────────────────────
        // ImGuiOGL линкован с тем же shared libglfw.dylib → видит то же окно
        if (m_ImGuiFactory)
            m_ImGuiLayer = m_ImGuiFactory->CreateImGuiLayer(m_Window->GetPlatformWindow()->GetNativeWindow());

        // ─── 5. Создать рендер-объекты (нужен GLAD, поэтому после шага 3) ───
        m_RenderApi = m_RenderResources.CreateRenderAPI();
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

        if (m_OGLRenderer && m_RenderFactory)
            m_RenderFactory->Delete(m_OGLRenderer);

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
