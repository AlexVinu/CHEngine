#pragma once

#include <Core.h>

#include "IPhysicsBody.h"
#include "IPhysicsShape.h"
#include "Timestep.h"

namespace CHEngine
{
    class IPhysicsContactListener
    {
    public:
        virtual ~IPhysicsContactListener() = default;
        virtual void OnContact(const PhysicsContactEvent& eventData) = 0;
    };

    class IPhysicsWorld
    {
    public:
        virtual ~IPhysicsWorld() = default;

        virtual void SetGravity(const glm::vec3& gravity) = 0;
        virtual void StepSimulation(Timestep deltaTime) = 0;

        virtual IPhysicsBody* CreateRigidBody(const PhysicsRigidBodyDesc& bodyDesc,
                                              const IPhysicsShape* shape) = 0;
        virtual void DestroyRigidBody(IPhysicsBody* body) = 0;

        virtual bool Raycast(const glm::vec3& origin, const glm::vec3& direction,
                             float maxDistance, PhysicsRaycastHit& outHit) = 0;
        virtual bool OverlapSphere(const glm::vec3& center, float radius,
                                   PhysicsOverlapResult& outResult) = 0;
        virtual bool OverlapBox(const glm::vec3& center, const glm::vec3& halfExtents,
                                const glm::quat& rotation, PhysicsOverlapResult& outResult) = 0;
        virtual bool OverlapCapsule(const glm::vec3& center, float radius, float halfHeight,
                                    const glm::quat& rotation, PhysicsOverlapResult& outResult) = 0;

        virtual void SetContactListener(IPhysicsContactListener* listener) = 0;
    };
}
