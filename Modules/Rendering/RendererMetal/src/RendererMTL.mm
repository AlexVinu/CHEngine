#include "RendererMTL.h"

#include <cstring>

#include <glm/matrix.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Render/IShader.h"

namespace CHModules
{
    RendererMTL::RendererMTL(CHEngine::IRenderApi* api)
        : m_Api(api)
    {
    }

    void RendererMTL::BeginScene() {}

    void RendererMTL::EndScene() {}

    void RendererMTL::Submit(const CHEngine::IVertexArray* mesh, const glm::mat4& transform)
    {
        (void)transform;
        if (m_Api)
            m_Api->DrawIndexed(mesh);
    }

    void RendererMTL::SubmitLines(const CHEngine::IVertexArray* mesh, const glm::mat4& transform)
    {
        (void)transform;
        if (m_Api)
            m_Api->DrawLines(mesh);
    }
}
