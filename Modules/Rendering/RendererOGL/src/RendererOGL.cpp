#include "RendererOGL.h"

#include <cstring>

#include <glm/matrix.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Render/IShader.h"

namespace CHModules
{
    RendererOGL::RendererOGL(CHEngine::IRenderApi* api)
        : m_Api(api)
    {
    }

    void RendererOGL::BeginScene() {}

    void RendererOGL::EndScene() {}

    void RendererOGL::Submit(const CHEngine::IVertexArray* mesh, const glm::mat4& transform)
    {
        (void)transform;
        if (m_Api)
            m_Api->DrawIndexed(mesh);
    }

    void RendererOGL::Submit(CHEngine::IShader* shader, CHEngine::IVertexArray* vertexArray,
                             const glm::mat4& transform, const CHEngine::UBOCamera& sceneCamera)
    {
        if (!m_Api || !shader || !vertexArray)
            return;

        shader->Bind();
        shader->SetUniformBlock(CHEngine::EUniformBlock::Camera, &sceneCamera, sizeof(sceneCamera));

        CHEngine::UBOObject objectUBO;
        const glm::mat4 normalMat = glm::transpose(glm::inverse(transform));
        std::memcpy(objectUBO.Transform,    glm::value_ptr(transform), sizeof(objectUBO.Transform));
        std::memcpy(objectUBO.NormalMatrix, glm::value_ptr(normalMat), sizeof(objectUBO.NormalMatrix));

        shader->SetUniformBlock(CHEngine::EUniformBlock::Object, &objectUBO, sizeof(objectUBO));

        // Material UBO already set by ApplyMaterial() before Submit() — do NOT override here
        m_Api->DrawIndexed(vertexArray);
    }

    void RendererOGL::SubmitLines(const CHEngine::IVertexArray* mesh, const glm::mat4& transform)
    {
        (void)transform;
        if (m_Api)
            m_Api->DrawLines(mesh);
    }
}
