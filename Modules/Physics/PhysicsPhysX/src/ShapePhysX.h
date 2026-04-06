#pragma once

#include "Physics/IPhysicsShape.h"
#include "Physics/PhysicsTypes.h"

namespace CHModules
{
    enum class PhysXShapeType : uint8_t
    {
        Box = 0,
        Sphere,
        Capsule
    };

    class ShapePhysX : public CHEngine::IPhysicsShape
    {
    public:
        explicit ShapePhysX(const glm::vec3& halfExtents)
            : m_Type(PhysXShapeType::Box), m_HalfExtents(halfExtents) {}

        explicit ShapePhysX(float radius)
            : m_Type(PhysXShapeType::Sphere), m_Radius(radius) {}

        ShapePhysX(float radius, float halfHeight)
            : m_Type(PhysXShapeType::Capsule), m_Radius(radius), m_HalfHeight(halfHeight) {}

        PhysXShapeType GetType() const { return m_Type; }
        const glm::vec3& GetHalfExtents() const { return m_HalfExtents; }
        float GetRadius() const { return m_Radius; }
        float GetHalfHeight() const { return m_HalfHeight; }

    private:
        PhysXShapeType m_Type = PhysXShapeType::Box;
        glm::vec3 m_HalfExtents{ 0.5f, 0.5f, 0.5f };
        float m_Radius = 0.5f;
        float m_HalfHeight = 0.5f;
    };
}
