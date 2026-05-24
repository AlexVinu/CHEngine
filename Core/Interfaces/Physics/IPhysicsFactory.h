#pragma once

#include <Core.h>

#include "IModuleFactory.h"
#include "IPhysicsContactListener.h"
#include "IPhysicsShape.h"
#include "PhysicsTypes.h"
#include "Handles.h"
#include "Timestep.h"

namespace CHEngine
{
    enum class EPhysicsAPI : uint8_t
    {
        PhysX = 0
    };

    struct IPhysicsFactory : IModuleFactory
    {
        virtual ~IPhysicsFactory() = default;

        // ── World lifecycle ────────────────────────────────────────────────────
        virtual PhysWorldHandle CreateWorld(const PhysicsWorldDesc& worldDesc) = 0;
        virtual void            Delete(PhysWorldHandle world) = 0;

        // ── World state ───────────────────────────────────────────────────────
        virtual void SetGravity(PhysWorldHandle world, const glm::vec3& gravity) = 0;
        virtual void StepSimulation(PhysWorldHandle world, Timestep deltaTime) = 0;
        virtual void SetContactListener(PhysWorldHandle world, IPhysicsContactListener* listener) = 0;

        // ── Shapes ────────────────────────────────────────────────────────────
        virtual PhysShapeHandle CreateShape(const PhysShapeDesc& shapeDesc) = 0;
        virtual void            Delete(PhysShapeHandle shape) = 0;

        // ── Bodies ────────────────────────────────────────────────────────────
        virtual PhysBodyHandle CreateRigidBody(PhysWorldHandle world,
                                               const PhysicsRigidBodyDesc& bodyDesc,
                                               PhysShapeHandle shape) = 0;
        virtual void           Delete(PhysBodyHandle body) = 0;

        virtual PhysicsBodyType  GetBodyType(PhysBodyHandle body) const = 0;
        virtual PhysicsTransform GetBodyTransform(PhysBodyHandle body) const = 0;
        virtual void             SetBodyTransform(PhysBodyHandle body, const PhysicsTransform& transform) = 0;
        virtual void             SetBodyKinematicTarget(PhysBodyHandle body, const PhysicsTransform& transform) = 0;
        virtual glm::vec3        GetBodyLinearVelocity(PhysBodyHandle body) const = 0;
        virtual void             SetBodyLinearVelocity(PhysBodyHandle body, const glm::vec3& velocity) = 0;
        virtual void             AddBodyForce(PhysBodyHandle body, const glm::vec3& force, PhysicsForceMode mode) = 0;

        // ── Queries (по миру) ─────────────────────────────────────────────────
        virtual bool Raycast(PhysWorldHandle world, const glm::vec3& origin, const glm::vec3& direction,
                             float maxDistance, PhysicsRaycastHit& outHit) = 0;
        virtual bool OverlapSphere(PhysWorldHandle world, const glm::vec3& center, float radius,
                                   PhysicsOverlapResult& outResult) = 0;
        virtual bool OverlapBox(PhysWorldHandle world, const glm::vec3& center, const glm::vec3& halfExtents,
                                const glm::quat& rotation, PhysicsOverlapResult& outResult) = 0;
        virtual bool OverlapCapsule(PhysWorldHandle world, const glm::vec3& center, float radius, float halfHeight,
                                    const glm::quat& rotation, PhysicsOverlapResult& outResult) = 0;

        // ── Информация о бэкенде ──────────────────────────────────────────────
        virtual EPhysicsAPI GetPhysicsApi() = 0;
        virtual bool        CheckIsWorking() = 0;
    };
}

DECLARE_MODULE_FACTORY()
