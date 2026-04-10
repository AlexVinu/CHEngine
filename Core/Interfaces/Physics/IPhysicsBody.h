#pragma once

#include <Core.h>

#include "PhysicsTypes.h"

namespace CHEngine
{
    enum class RigidBodySyncMode : uint8_t
    {
        Auto = 0,
        ReadFromPhysics = 1,
        WriteToPhysics = 2,
        ReadWrite = 3
    };

    class IPhysicsBody
    {
    public:
        virtual ~IPhysicsBody() = default;

        virtual PhysicsBodyType GetType() const = 0;

        virtual PhysicsTransform GetTransform() const = 0;
        virtual void SetTransform(const PhysicsTransform& transform) = 0;
        virtual void SetKinematicTarget(const PhysicsTransform& transform) = 0;

        virtual glm::vec3 GetLinearVelocity() const = 0;
        virtual void SetLinearVelocity(const glm::vec3& velocity) = 0;

        virtual void AddForce(const glm::vec3& force, PhysicsForceMode mode) = 0;
    };
}
