#include "WindowGLFW.h"

#include <Log/Log.h>

#if defined(CHE_PLATFORM_APPLE)
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>
#endif

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
                           CHEngine::ErrorCallbackFn errorCallbackFn, CHEngine::ERenderAPI renderApi)
        : m_RenderAPI(renderApi), m_Width(width), m_Height(height)
    {
        CHE_CORE_ASSERT(renderApi != CHEngine::ERenderAPI::NONE, "Render API was not set");

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

        if (renderApi == CHEngine::ERenderAPI::OPENGL)
        {
            // OpenGL version: macOS caps at 4.1 (Apple limitation),
            // Windows/Linux request 4.5 for broad compatibility.
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
#ifdef CHE_PLATFORM_APPLE
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
            glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#else
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
#endif
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        }
        else
        {
            // Vulkan / Metal / DirectX — GLFW не создаёт графический контекст
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        }

        m_Window = glfwCreateWindow((int)width, (int)height, title, nullptr, nullptr);
        if (!m_Window) {
            CHE_CORE_ERROR("Failed to create GLFW window");
            glfwTerminate();
            return;
        }

        // GL context и VSync — только для OpenGL
        if (renderApi == CHEngine::ERenderAPI::OPENGL) {
            glfwMakeContextCurrent(m_Window);
            glfwSwapInterval(1);
        }
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
        // SwapBuffers только для OpenGL — Vulkan/Metal делают present сами
        if (m_Window && m_RenderAPI == CHEngine::ERenderAPI::OPENGL)
            glfwSwapBuffers(m_Window);
    }

    void WindowGLFW::PollEvents()
    {
        glfwPollEvents();

        // macOS native fullscreen (green button) doesn't always fire the resize
        // callback — poll the actual size every frame and trigger it manually.
        if (m_Window && m_Context.ResizeCallback)
        {
            int w = 0, h = 0;
            glfwGetWindowSize(m_Window, &w, &h);
            if (w > 0 && h > 0 &&
                (static_cast<uint32_t>(w) != m_Width || static_cast<uint32_t>(h) != m_Height))
            {
                m_Width  = static_cast<uint32_t>(w);
                m_Height = static_cast<uint32_t>(h);
                m_Context.ResizeCallback(m_Context.UserPointer, w, h);
            }
        }
    }

    void WindowGLFW::SetVSync(bool enabled)
    {
        // glfwSwapInterval работает только с GL контекстом
        if (m_RenderAPI == CHEngine::ERenderAPI::OPENGL)
            glfwSwapInterval(enabled ? 1 : 0);
    }

	void WindowGLFW::SetMouse(bool active)
	{
        if (active)
        {
            glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            if (glfwRawMouseMotionSupported())
                glfwSetInputMode(m_Window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
        }
        else
        {
            glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            if (glfwRawMouseMotionSupported())
                glfwSetInputMode(m_Window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        }
	}

	CHEngine::RendererInitInfo WindowGLFW::GetRenderInitInfo(CHEngine::ERenderAPI render_api) const
    {
        CHEngine::RendererInitInfo info{};

        switch (render_api)
        {
        case CHEngine::ERenderAPI::OPENGL:
            // reinterpret_cast: function pointer → void* (POSIX гарантирует совместимость)
            info.OpenGL.Loader = reinterpret_cast<void*>(glfwGetProcAddress);
            break;

        case CHEngine::ERenderAPI::VULKAN:
            // Передаём GLFWwindow* — RendererVK создаст VkSurfaceKHR через glfwCreateWindowSurface
            info.Vulkan.WindowHandle = m_Window;
            break;

        case CHEngine::ERenderAPI::METAL:
#if defined(CHE_PLATFORM_APPLE)
            // glfwGetCocoaWindow возвращает NSWindow*, из которого Metal-рендерер
            // получит contentView и создаст CAMetalLayer
            info.Metal.NSWindow = glfwGetCocoaWindow(m_Window);
#else
            CHE_CORE_ERROR("GetRenderInitInfo: Metal is not supported on this platform");
#endif
            break;

        case CHEngine::ERenderAPI::DIRECTX11:
        case CHEngine::ERenderAPI::DIRECTX12:
            CHE_CORE_ERROR("GetRenderInitInfo: API {} is not implemented yet", (int)render_api);
            break;
        case CHEngine::ERenderAPI::NONE:
        default:
            CHE_CORE_ERROR("GetRenderInitInfo: Render API is not set");
            break;
        }

        return info;
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

    void WindowGLFW::GetWindowPos(int& x, int& y) const
    {
        if (m_Window)
            glfwGetWindowPos(m_Window, &x, &y);
        else
            x = y = 0;
    }

    void WindowGLFW::SetWindowPos(int x, int y)
    {
        if (m_Window)
            glfwSetWindowPos(m_Window, x, y);
    }

    void WindowGLFW::SetWindowSize(uint32_t w, uint32_t h)
    {
        if (m_Window && w > 0 && h > 0)
            glfwSetWindowSize(m_Window, static_cast<int>(w), static_cast<int>(h));
    }

    void WindowGLFW::SetWindowContext(const CHEngine::WindowContext& context)
    {
        m_Context = context;
        glfwSetWindowUserPointer(m_Window, this);

        glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* window, int w, int h) {
            if (w <= 0 || h <= 0) return;
            auto* self = (WindowGLFW*)glfwGetWindowUserPointer(window);
            if (self->m_Context.ResizeCallback)
                self->m_Context.ResizeCallback(self->m_Context.UserPointer, w, h);
        });

        glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* window) {
            auto* self = (WindowGLFW*)glfwGetWindowUserPointer(window);
            if (self->m_Context.CloseCallback)
                self->m_Context.CloseCallback(self->m_Context.UserPointer);
        });

        glfwSetKeyCallback(m_Window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
            auto* self = (WindowGLFW*)glfwGetWindowUserPointer(window);
            if (self->m_Context.KeyCallback)
                self->m_Context.KeyCallback(self->m_Context.UserPointer, key, scancode, (int)ConvertFromGLFW(action), mods);
        });

        glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* window, int button, int action, int mods) {
            auto* self = (WindowGLFW*)glfwGetWindowUserPointer(window);
            if (self->m_Context.MouseButtonCallback)
                self->m_Context.MouseButtonCallback(self->m_Context.UserPointer, button, (int)ConvertFromGLFW(action), mods);
        });

        glfwSetScrollCallback(m_Window, [](GLFWwindow* window, double x, double y) {
            auto* self = (WindowGLFW*)glfwGetWindowUserPointer(window);
            self->m_ScrollDelta += (float)y;
            if (self->m_Context.ScrollCallback)
                self->m_Context.ScrollCallback(self->m_Context.UserPointer, (float)x, (float)y);
        });

        glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, double x, double y) {
            auto* self = (WindowGLFW*)glfwGetWindowUserPointer(window);
            if (self->m_Context.CursorPosCallback)
                self->m_Context.CursorPosCallback(self->m_Context.UserPointer, (float)x, (float)y);
        });
    }

}
