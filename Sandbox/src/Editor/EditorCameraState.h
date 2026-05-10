#pragma once

#include <glm/glm.hpp>

namespace Sandbox {

struct EditorCameraState
{
    glm::vec3 OrbitTarget{ 0.0f, 0.0f, 0.0f };
    float OrbitDist = 8.0f;
    bool FollowObject = false;
};

} // namespace Sandbox
