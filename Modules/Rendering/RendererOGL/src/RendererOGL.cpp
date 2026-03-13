#include "RendererOGL.h"


namespace CHModules
{
    static bool s_GLADInitialized = false;

    RendererOGL::RendererOGL(uint32_t width, uint32_t height)
    {
        Init(width, height);
    }

    RendererOGL::~RendererOGL() = default;

    void RendererOGL::Init(uint32_t width, uint32_t height)
	{

        if (!s_GLADInitialized) {
            int success = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
            CHE_CORE_ASSERT(success, "Failed to initialize GLAD");
            s_GLADInitialized = true;
        }

        glViewport(0, 0, width, height);
	}
	void RendererOGL::Shutdown()
	{
        // Nothing to shut down at the renderer level yet.
	}

    void RendererOGL::SetViewport(uint32_t width, uint32_t height)
    {
        glViewport(0, 0, width, height);
    }
}

