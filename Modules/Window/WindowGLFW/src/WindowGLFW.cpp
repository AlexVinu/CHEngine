#include "WindowGLFW.h"

#include <Log/Log.h>

namespace CHModules {

    static bool s_GLFWInitialized = false;
    static CHEngine::ErrorCallbackFn s_ErrorCallbackFn = nullptr;

    CHEngine::EventType WindowGLFW::ConvertFromGLFW(int action)
    {
        switch (action)
        {
        case GLFW_PRESS:   return CHEngine::EventType::KeyPressed;
        case GLFW_RELEASE: return CHEngine::EventType::KeyReleased;
        case GLFW_REPEAT:  return CHEngine::EventType::KeyRepeat;
        }
        return CHEngine::EventType::None;
    }

    WindowGLFW::WindowGLFW(uint32_t width, uint32_t height, const char* title,
                           CHEngine::ErrorCallbackFn errorCallbackFn, CHEngine::ERenderAPI renderApi)
        : m_Width(width), m_Height(height)
    {
        CHE_CORE_ASSERT(renderApi != CHEngine::ERenderAPI::NONE, "Render API was not set");

        if (!s_GLFWInitialized) {
            int success = glfwInit();
            CHE_CORE_ASSERT(success, "Failed to initialize GLFW");
            s_GLFWInitialized = true;
            s_ErrorCallbackFn = errorCallbackFn;
            glfwSetErrorCallback([](int error, const char* description) {
                if (s_ErrorCallbackFn)
                    s_ErrorCallbackFn(error, description);
            });
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
#ifdef CHE_PLATFORM_APPLE
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#else
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
#endif
        if(renderApi == CHEngine::ERenderAPI::OPENGL)
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        else
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        m_Window = glfwCreateWindow((int)width, (int)height, title, nullptr, nullptr);
        if (!m_Window) {
            CHE_CORE_ERROR("Failed to create GLFW window");
            glfwTerminate();
            return;
        }

        glfwMakeContextCurrent(m_Window);
        glfwSwapInterval(1);  // VSync включён по умолчанию
    }

    WindowGLFW::~WindowGLFW()
    {
        Shutdown();
    }

    void WindowGLFW::Shutdown()
    {
        if (m_Window) {
            glfwDestroyWindow(m_Window);
            m_Window = nullptr;
        }
        glfwTerminate();
        s_GLFWInitialized = false;
    }

    void WindowGLFW::SwapBuffers()
    {
        glfwSwapBuffers(m_Window);
    }

    void WindowGLFW::PollEvents()
    {
        glfwPollEvents();
    }

    void WindowGLFW::SetVSync(bool enabled)
    {
        glfwSwapInterval(enabled ? 1 : 0);
    }

    CHEngine::RendererInitInfo WindowGLFW::GetRenderInitInfo(CHEngine::ERenderAPI render_api) const
    {
        // Надо думать как сделать универсальное заполнение для все видов апи
        CHEngine::RendererInitInfo info;
        info.Loader = nullptr;
        info.Height = m_Height; info.Width = m_Width;

        switch (render_api)
        {
        case CHEngine::ERenderAPI::NONE: CHE_ASSERT(true, "Render was not set");
            break;
        case CHEngine::ERenderAPI::OPENGL: info.Loader = (ProcLoader)glfwGetProcAddress;
            break;
        case CHEngine::ERenderAPI::VULKAN: 
            break;
        case CHEngine::ERenderAPI::METALL:
            break;
        case CHEngine::ERenderAPI::DIRECTX11:
            break;
        case CHEngine::ERenderAPI::DIRECTX12:
            break;
        default:
            break;
        }

        return info;
    }

    void WindowGLFW::SetWindowContext(const CHEngine::WindowContext& context)
    {
        m_Context = context;
        glfwSetWindowUserPointer(m_Window, &m_Context);

        glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* window, int w, int h) {
            auto* ctx = (CHEngine::WindowContext*)glfwGetWindowUserPointer(window);
            if (ctx && ctx->ResizeCallback)
                ctx->ResizeCallback(ctx->UserPointer, w, h);
        });

        glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* window) {
            auto* ctx = (CHEngine::WindowContext*)glfwGetWindowUserPointer(window);
            if (ctx && ctx->CloseCallback)
                ctx->CloseCallback(ctx->UserPointer);
        });

        glfwSetKeyCallback(m_Window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
            auto* ctx = (CHEngine::WindowContext*)glfwGetWindowUserPointer(window);
            if (ctx && ctx->KeyCallback) {
                CHEngine::EventType _action = ConvertFromGLFW(action);
                ctx->KeyCallback(ctx->UserPointer, key, scancode, (int)_action, mods);
            }
        });

        glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* window, int button, int action, int mods) {
            auto* ctx = (CHEngine::WindowContext*)glfwGetWindowUserPointer(window);
            if (ctx && ctx->MouseButtonCallback) {
                CHEngine::EventType _action = ConvertFromGLFW(action);
                ctx->MouseButtonCallback(ctx->UserPointer, button, (int)_action, mods);
            }
        });

        glfwSetScrollCallback(m_Window, [](GLFWwindow* window, double x, double y) {
            auto* ctx = (CHEngine::WindowContext*)glfwGetWindowUserPointer(window);
            if (ctx && ctx->ScrollCallback)
                ctx->ScrollCallback(ctx->UserPointer, (float)x, (float)y);
        });

        glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, double x, double y) {
            auto* ctx = (CHEngine::WindowContext*)glfwGetWindowUserPointer(window);
            if (ctx && ctx->CursorPosCallback)
                ctx->CursorPosCallback(ctx->UserPointer, (float)x, (float)y);
        });
    }

}
