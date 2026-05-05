#include "RendererVK.h"

#include <cstring>

#include <glm/matrix.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Render/IShader.h"

namespace CHModules
{
    RendererVK::RendererVK(CHEngine::IRenderApi* api)
        : m_Api(api)
    {
    }

    void RendererVK::BeginScene() {}

    void RendererVK::EndScene() {}

    void RendererVK::Submit(const CHEngine::IVertexArray* mesh, const glm::mat4& transform)
    {
        (void)transform;
        if (m_Api)
            m_Api->DrawIndexed(mesh);
    }

    void RendererVK::SubmitLines(const CHEngine::IVertexArray* mesh, const glm::mat4& transform)
    {
        (void)transform;
        if (m_Api)
            m_Api->DrawLines(mesh);
    }
}
