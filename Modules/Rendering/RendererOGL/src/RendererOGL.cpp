#include "RendererOGL.h"


namespace CHModules
{
    static bool s_GLFWInitialized = false;
    static bool s_GLADInitialized = false;
    static CHEngine::ErrorCallbackFn s_ErrorCallbackFn;

    CHEngine::EventType RendererOGL::ConvertFromGLFW(int action)
    {
        switch (action)
        {
        case GLFW_PRESS:   return CHEngine::EventType::KeyPressed;
        case GLFW_RELEASE: return CHEngine::EventType::KeyReleased;
        case GLFW_REPEAT:  return CHEngine::EventType::KeyRepeat;
        }

        return CHEngine::EventType::None;
    }

    RendererOGL::RendererOGL(const unsigned int width, const unsigned int height, const char* title, CHEngine::ErrorCallbackFn errorCallbackFn)
    {
        Init(width, height, title, errorCallbackFn);
    }

    RendererOGL::~RendererOGL()
    {
        Shutdown();
    }

    void RendererOGL::SetViewport(uint32_t width, uint32_t height)
    {
        glViewport(0, 0, width, height);
    }

    void RendererOGL::Init(const unsigned int width, const unsigned int height, const char* title, CHEngine::ErrorCallbackFn errorCallbackFn)
	{
        if (!s_GLFWInitialized) {
            int success = glfwInit();
            CHE_CORE_ASSERT(success, "Failed to initialize GLFW");
            s_GLFWInitialized = true;
            s_ErrorCallbackFn = errorCallbackFn;
            glfwSetErrorCallback([](int error, const char* description)
                {
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
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        
        m_Window = glfwCreateWindow(width, height, title, nullptr, nullptr);
        if (!m_Window) {
            CHE_CORE_ERROR("Failed to create GLFW window");
            glfwTerminate();
        }

        glfwMakeContextCurrent(m_Window);

        if (!s_GLADInitialized) {
            int success = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
            CHE_CORE_ASSERT(success, "Failed to initialize GLAD");
            s_GLADInitialized = true;
        }

        glViewport(0, 0, width, height);
	}
	void RendererOGL::Shutdown()
	{
        if (m_Window) {
            glfwDestroyWindow(m_Window);
            m_Window = nullptr;
        }
        glfwTerminate();
	}
	void RendererOGL::SwapBuffers()
	{
        glfwSwapBuffers(m_Window);
	}
	void RendererOGL::PollEvents()
	{
        glfwPollEvents();
	}
    void RendererOGL::SetVSync(bool enabled)
    {
        if (enabled)
            glfwSwapInterval(1);
        else
            glfwSwapInterval(0);
    }
    void RendererOGL::SetWindowContext(const CHEngine::RendererWindowContext& context)
    {
        m_WindowContext = context;

        glfwSetWindowUserPointer(m_Window, &m_WindowContext);

        glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* window, int w, int h)
            {
                auto* ctx = (CHEngine::RendererWindowContext*)glfwGetWindowUserPointer(window);
                if (ctx && ctx->ResizeCallback)
                    ctx->ResizeCallback(ctx->UserPointer, w, h);
            });

        glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* window)
            {
                auto* ctx = (CHEngine::RendererWindowContext*)glfwGetWindowUserPointer(window);
                if (ctx && ctx->CloseCallback)
                    ctx->CloseCallback(ctx->UserPointer);
            });

        glfwSetKeyCallback(m_Window, [](GLFWwindow* window, int key, int scancode, int action, int mods)
            {
                auto* ctx = (CHEngine::RendererWindowContext*)glfwGetWindowUserPointer(window);
                if (ctx && ctx->KeyCallback)
                {
                    CHEngine::EventType _action = ConvertFromGLFW(action);
                    ctx->KeyCallback(ctx->UserPointer, key, scancode, (int)_action, mods);
                }
            });

        glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* window, int button, int action, int mods)
            {
                auto* ctx = (CHEngine::RendererWindowContext*)glfwGetWindowUserPointer(window);
                if (ctx && ctx->MouseButtonCallback)
                {
                    CHEngine::EventType _action = ConvertFromGLFW(action);
                    ctx->MouseButtonCallback(ctx->UserPointer, button, (int)_action, mods);
                }
            });

        glfwSetScrollCallback(m_Window, [](GLFWwindow* window, double x, double y)
            {
                auto* ctx = (CHEngine::RendererWindowContext*)glfwGetWindowUserPointer(window);
                if (ctx && ctx->ScrollCallback)
                    ctx->ScrollCallback(ctx->UserPointer, (float)x, (float)y);
            });

        glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, double x, double y)
            {
                auto* ctx = (CHEngine::RendererWindowContext*)glfwGetWindowUserPointer(window);
                if (ctx && ctx->CursorPosCallback)
                    ctx->CursorPosCallback(ctx->UserPointer, (float)x, (float)y);
            });
    }
}

