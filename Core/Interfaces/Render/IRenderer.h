#pragma once

#include <Core.h>

#include <glm/mat4x4.hpp>

#include "IVertexArray.h"
#include "UniformBlocks.h"

namespace CHEngine
{
    class IShader;

    class IRenderer
    {
    public:
        virtual ~IRenderer() = default;

        virtual void BeginScene() = 0;
        virtual void Submit(const IVertexArray* mesh, const glm::mat4& transform) = 0;
        virtual void SubmitLines(const IVertexArray* mesh, const glm::mat4& transform) = 0;
        virtual void Submit(IShader* shader, IVertexArray* vertexArray, const glm::mat4& transform,
                            const UBOCamera& sceneCamera) = 0;
        virtual void EndScene() = 0;
    };
}
