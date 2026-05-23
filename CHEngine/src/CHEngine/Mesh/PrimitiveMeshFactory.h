#pragma once

#include "MeshRef.h"

#include <glm/glm.hpp>

namespace CHEngine {

class CHENGINE_API PrimitiveMeshFactory
{
public:
    static MeshRef CreateCube(float size, const glm::vec3& color);
    static MeshRef CreateSphere(float radius, uint32_t segments, uint32_t slices, const glm::vec3& color);

    // Billboard quad for sphere impostor rendering.
    // The mesh has 4 verts with Position in [-1,1]x[-1,1] local space.
    // Actual world radius is encoded in the entity's transform scale:
    //   worldRadius = length(Transform[0].xyz)   (i.e. scale == radius)
    static MeshRef CreateSphereImpostor(const glm::vec3& color);
};

} // namespace CHEngine
