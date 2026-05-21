#pragma once

#include "Physics/IPhysicsWorld.h"

#include <PxPhysicsAPI.h>

#include <CheStl/MemoryTypes.h>
#include <vector>

namespace CHModules
{
    class BodyPhysX;

    class PhysicsWorldPhysX : public CHEngine::IPhysicsWorld
    {
    public:
        PhysicsWorldPhysX(physx::PxPhysics& physics, const CHEngine::PhysicsWorldDesc& worldDesc);
        ~PhysicsWorldPhysX() override;

        void SetGravity(const glm::vec3& gravity) override;
        void StepSimulation(CHEngine::Timestep deltaTime) override;

        CHEngine::IPhysicsBody* CreateRigidBody(const CHEngine::PhysicsRigidBodyDesc& bodyDesc,
                                                const CHEngine::IPhysicsShape* shape) override;
        void DestroyRigidBody(CHEngine::IPhysicsBody* body) override;

        bool Raycast(const glm::vec3& origin, const glm::vec3& direction,
                     float maxDistance, CHEngine::PhysicsRaycastHit& outHit) override;

        bool OverlapSphere(const glm::vec3& center, float radius,
            CHEngine::PhysicsOverlapResult& outResult) override;

        bool OverlapBox(const glm::vec3& center, const glm::vec3& halfExtents,
            const glm::quat& rotation, CHEngine::PhysicsOverlapResult& outResult) override;

        bool OverlapCapsule(const glm::vec3& center, float radius, float halfHeight,
            const glm::quat& rotation, CHEngine::PhysicsOverlapResult& outResult) override;

        void SetContactListener(CHEngine::IPhysicsContactListener* listener) override;

        bool IsValid() const;

    private:
        class ContactCallbackPhysX : public physx::PxSimulationEventCallback
        {
        public:
            explicit ContactCallbackPhysX(PhysicsWorldPhysX* owner);

            void onConstraintBreak(physx::PxConstraintInfo*, physx::PxU32) override;
            void onWake(physx::PxActor**, physx::PxU32) override;
            void onSleep(physx::PxActor**, physx::PxU32) override;
            void onTrigger(physx::PxTriggerPair*, physx::PxU32) override;
            void onAdvance(const physx::PxRigidBody* const*, const physx::PxTransform*, const physx::PxU32) override;

            void onContact(const physx::PxContactPairHeader& pairHeader,
                const physx::PxContactPair* pairs,
                physx::PxU32 nbPairs) override;
        private:
            PhysicsWorldPhysX* m_Owner = nullptr;
        };

        BodyPhysX* FindBodyByActor(const physx::PxRigidActor* actor) const;

        physx::PxPhysics& m_Physics;
        physx::PxDefaultCpuDispatcher* m_Dispatcher = nullptr;
        physx::PxScene* m_Scene = nullptr;

        std::vector<Scope<BodyPhysX>> m_Bodies;
        Scope<ContactCallbackPhysX> m_ContactCallback;
        CHEngine::IPhysicsContactListener* m_ContactListener = nullptr;
    };
}
