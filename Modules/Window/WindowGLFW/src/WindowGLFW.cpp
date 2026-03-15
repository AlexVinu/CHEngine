#include "WindowGLFW.h"

#include <Log/Log.h>

namespace CHModules {

    static int  s_GLFWRefCount     = 0;
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
                           CHEngine::ErrorCallbackFn errorCallbackFn)
        : m_Width(width), m_Height(height)
    {
        if (s_GLFWRefCount == 0) {
            [[maybe_unused]] int success = glfwInit();
            CHE_CORE_ASSERT(success, "Failed to initialize GLFW");
            s_ErrorCallbackFn = errorCallbackFn;
            glfwSetErrorCallback([](int error, const char* description) {
                if (s_ErrorCallbackFn)
                    s_ErrorCallbackFn(error, description);
            });
        }
        ++s_GLFWRefCount;

        // OpenGL version: macOS caps at 4.1 (Apple limitation),
        // Windows/Linux request 4.5 for broad compatibility.
        // Shaders use #version 330 core, so 3.3+ would suffice,
        // but 4.x gives access to DSA, compute shaders, etc. if needed.
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
#ifdef CHE_PLATFORM_APPLE
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#else
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
#endif
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

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

        --s_GLFWRefCount;
        if (s_GLFWRefCount <= 0) {
            glfwTerminate();
            s_GLFWRefCount = 0;
        }
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

    bool WindowGLFW::IsKeyDown(int key) const
    {
        // GLFW key codes below GLFW_KEY_SPACE (32) are invalid and trigger errors
        if (!m_Window || key < GLFW_KEY_SPACE) return false;
        return glfwGetKey(m_Window, key) == GLFW_PRESS;
    }

    bool WindowGLFW::IsMouseButtonDown(int button) const
    {
        return m_Window && glfwGetMouseButton(m_Window, button) == GLFW_PRESS;
    }

    void WindowGLFW::GetMousePosition(float& x, float& y) const
    {
        if (m_Window) {
            double dx, dy;
            glfwGetCursorPos(m_Window, &dx, &dy);
            x = static_cast<float>(dx);
            y = static_cast<float>(dy);
        } else {
            x = y = 0.0f;
        }
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
