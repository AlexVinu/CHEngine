#pragma once

#include "Physics/IPhysicsBody.h"

#include <PxPhysicsAPI.h>

namespace CHModules
{
    class BodyPhysX : public CHEngine::IPhysicsBody
    {
    public:
        BodyPhysX(physx::PxRigidActor* actor, CHEngine::PhysicsBodyType bodyType);
        ~BodyPhysX() override;

        CHEngine::PhysicsBodyType GetType() const override { return m_BodyType; }

        CHEngine::PhysicsTransform GetTransform() const override;
        void SetTransform(const CHEngine::PhysicsTransform& transform) override;
        void SetKinematicTarget(const CHEngine::PhysicsTransform& transform) override;

        glm::vec3 GetLinearVelocity() const override;
        void SetLinearVelocity(const glm::vec3& velocity) override;

        void AddForce(const glm::vec3& force, CHEngine::PhysicsForceMode mode) override;

        physx::PxRigidActor* GetActor() const { return m_Actor; }

    private:
        physx::PxRigidActor* m_Actor = nullptr;
        CHEngine::PhysicsBodyType m_BodyType = CHEngine::PhysicsBodyType::Dynamic;
    };
}
