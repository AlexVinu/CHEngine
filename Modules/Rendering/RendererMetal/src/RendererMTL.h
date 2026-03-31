#pragma once

#include "Render/IRenderApi.h"
#include "Render/IRenderer.h"

namespace CHModules
{
    class RendererMTL : public CHEngine::IRenderer
    {
    public:
        explicit RendererMTL(CHEngine::IRenderApi* api);
        ~RendererMTL() override = default;

        void BeginScene() override;
        void Submit(const CHEngine::IVertexArray* mesh, const glm::mat4& transform) override;
        void SubmitLines(const CHEngine::IVertexArray* mesh, const glm::mat4& transform) override;
        void Submit(CHEngine::IShader* shader, CHEngine::IVertexArray* vertexArray, const glm::mat4& transform,
                      const CHEngine::UBOCamera& sceneCamera) override;
        void EndScene() override;

    private:
        CHEngine::IRenderApi* m_Api = nullptr;
    };
}
