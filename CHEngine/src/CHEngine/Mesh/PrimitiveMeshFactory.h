#pragma once

#include "Mesh.h"

#include <glm/glm.hpp>

namespace CHEngine {

class CHENGINE_API PrimitiveMeshFactory
{
public:
    static Mesh CreateCube(float size, const glm::vec3& color);
    static Mesh CreateSphere(float radius, uint32_t segments, uint32_t slices, const glm::vec3& color);
};

} // namespace CHEngine
