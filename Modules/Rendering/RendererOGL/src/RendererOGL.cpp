#include "RendererOGL.h"

#include <Log/Log.h>

namespace CHModules
{
    static bool s_GLADInitialized = false;

    void RendererOGL::Init(void* nativeWindow)
    {
        if (!s_GLADInitialized) {
            // GLFW контекст уже сделан current в WindowGLFW::WindowGLFW().
            // nativeWindow передаётся на случай если нужен cast, но
            // glfwGetProcAddress работает с текущим контекстом.
            (void)nativeWindow;
            int success = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
            CHE_CORE_ASSERT(success, "Failed to initialize GLAD");
            s_GLADInitialized = true;

            CHE_CORE_INFO("OpenGL initialized: {0}", (const char*)glGetString(GL_VERSION));
        }
    }

    void RendererOGL::SetViewport(uint32_t width, uint32_t height)
    {
        glViewport(0, 0, (GLsizei)width, (GLsizei)height);
    }
}
