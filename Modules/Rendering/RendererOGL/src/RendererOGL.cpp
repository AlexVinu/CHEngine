#include "RendererOGL.h"

#include <Log/Log.h>

namespace CHModules
{
    static bool s_GLADInitialized = false;

    void RendererOGL::Init(const CHEngine::RendererInitInfo& init_info)
    {
        if (!s_GLADInitialized) {
            int success = gladLoadGLLoader((GLADloadproc)init_info.OpenGL.Loader);
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
