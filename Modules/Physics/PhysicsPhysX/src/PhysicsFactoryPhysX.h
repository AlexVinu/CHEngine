#pragma once

#include "Physics/IPhysicsFactory.h"

#include <PxPhysicsAPI.h>

namespace CHModules
{
    class PhysicsFactoryPhysX : public CHEngine::IPhysicsFactory
    {
    public:
        PhysicsFactoryPhysX();
        ~PhysicsFactoryPhysX();

        CHEngine::IPhysicsWorld* CreateWorld(const CHEngine::PhysicsWorldDesc& worldDesc) override;

        CHEngine::IPhysicsShape* CreateBoxShape(const glm::vec3& halfExtents) override;
        CHEngine::IPhysicsShape* CreateSphereShape(float radius) override;
        CHEngine::IPhysicsShape* CreateCapsuleShape(float radius, float halfHeight) override;

        void Delete(CHEngine::IPhysicsWorld* world) override;
        void Delete(CHEngine::IPhysicsShape* shape) override;

        CHEngine::EPhysicsAPI GetPhysicsApi() override;
        bool CheckIsWorking() override;

        CHEngine::ModuleType GetType() const override;

    private:
        physx::PxDefaultAllocator     m_Allocator;
        physx::PxDefaultErrorCallback m_ErrorCallback;
        physx::PxFoundation*          m_Foundation = nullptr;
        physx::PxPhysics*             m_Physics    = nullptr;
    };
}
