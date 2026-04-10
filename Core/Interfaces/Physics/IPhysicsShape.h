#pragma once

#include <Core.h>

namespace CHEngine
{
    enum class PhysicsColliderShapeType : uint8_t
    {
        Box = 0,
        Sphere = 1,
        Capsule = 2
    };

    struct PhysicsColliderShapeDesc
    {
        PhysicsColliderShapeType Type = PhysicsColliderShapeType::Box;
        glm::vec3 HalfExtents = { 0.5f, 0.5f, 0.5f };
        float Radius = 0.5f;
        float HalfHeight = 0.5f;
    };

    class IPhysicsShape
    {
    public:
        virtual ~IPhysicsShape() = default;
    };
}
