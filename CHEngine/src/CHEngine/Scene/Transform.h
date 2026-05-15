#pragma once

#include <glm/glm.hpp>

namespace CHEngine
{
    struct CHENGINE_API Transform
    {
        glm::vec3 Position = { 0.0f, 0.0f, 0.0f };
        glm::vec3 Rotation = { 0.0f, 0.0f, 0.0f }; // Euler angles in degrees
        glm::vec3 Scale = { 1.0f, 1.0f, 1.0f };

        glm::mat4 GetMatrix() const;
        glm::mat4 GetNormalMatrix() const;
    };
}
