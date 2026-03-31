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

    void RendererMTL::Submit(CHEngine::IShader* shader, CHEngine::IVertexArray* vertexArray,
                             const glm::mat4& transform, const CHEngine::UBOCamera& sceneCamera)
    {
        if (!m_Api || !shader || !vertexArray)
            return;

        shader->Bind();
        shader->SetUniformBlock(CHEngine::EUniformBlock::Camera, &sceneCamera, sizeof(sceneCamera));

        CHEngine::UBOObject objectUBO;
        const glm::mat4 normalMat = glm::transpose(glm::inverse(transform));
        std::memcpy(objectUBO.Transform, glm::value_ptr(transform), sizeof(objectUBO.Transform));
        std::memcpy(objectUBO.NormalMatrix, glm::value_ptr(normalMat), sizeof(objectUBO.NormalMatrix));

        shader->SetUniformBlock(CHEngine::EUniformBlock::Object, &objectUBO, sizeof(objectUBO));

        CHEngine::UBOMaterial defaultMaterial;
        shader->SetUniformBlock(CHEngine::EUniformBlock::Material, &defaultMaterial, sizeof(defaultMaterial));

        m_Api->DrawIndexed(vertexArray);
    }

    void RendererMTL::SubmitLines(const CHEngine::IVertexArray* mesh, const glm::mat4& transform)
    {
        (void)transform;
        if (m_Api)
            m_Api->DrawLines(mesh);
    }
}
