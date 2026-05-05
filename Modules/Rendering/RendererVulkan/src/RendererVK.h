#pragma once

#include "Render/IRenderApi.h"
#include "Render/IRenderer.h"

namespace CHModules
{
    class RendererVK : public CHEngine::IRenderer
    {
    public:
        explicit RendererVK(CHEngine::IRenderApi* api);
        ~RendererVK() override = default;

        void BeginScene() override;
        void Submit(const CHEngine::IVertexArray* mesh, const glm::mat4& transform) override;
        void SubmitLines(const CHEngine::IVertexArray* mesh, const glm::mat4& transform) override;
        void EndScene() override;

    };
}
