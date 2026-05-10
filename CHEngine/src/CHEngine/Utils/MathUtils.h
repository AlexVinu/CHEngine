#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace CHEngine::MathUtils {

    inline glm::quat QuatFromEulerDegrees(const glm::vec3& euler_degrees)
    {
        return glm::quat(glm::radians(euler_degrees));
    }

    inline glm::vec3 ForwardFromQuat(const glm::quat& q)
    {
        return glm::normalize(glm::rotate(q, glm::vec3(0.0f, 0.0f, -1.0f)));
    }

    inline glm::vec3 UpFromQuat(const glm::quat& q)
    {
        return glm::normalize(glm::rotate(q, glm::vec3(0.0f, 1.0f, 0.0f)));
    }

    inline glm::mat4 ViewMatrixFromTransform(const Transform& transform)
    {
        const glm::quat orientation = QuatFromEulerDegrees(transform.Rotation);
        const glm::vec3 forward = ForwardFromQuat(orientation);
        const glm::vec3 up = UpFromQuat(orientation);
        return glm::lookAt(transform.Position, transform.Position + forward, up);
    }

} // namespace CHEngine::MathUtils