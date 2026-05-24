#pragma once

#include <Core.h>

#include <variant>

namespace CHEngine
{
    enum class PhysShapeType : uint8_t
    {
        Box = 0,
        Sphere = 1,
        Capsule = 2
    };

	struct BoxDesc { glm::vec3 halfExtents{ 0.5f, 0.5f, 0.5f }; };
	struct SphereDesc { float radius{ 0.5f }; };
	struct CapsuleDesc { float radius{ 0.5f }; float halfHeight{ 0.5f }; };

	using PhysShapeDesc = std::variant<BoxDesc, SphereDesc, CapsuleDesc>;

    struct PhysicsColliderShapeDesc
    {
        PhysShapeType Type = PhysShapeType::Box;
        glm::vec3 HalfExtents = { 0.5f, 0.5f, 0.5f };
        float Radius = 0.5f;
        float HalfHeight = 0.5f;
    };
}
