#pragma once

#include "Physics/IPhysicsFactory.h"

namespace CHModules
{
    class PhysicsFactoryPhysX : public CHEngine::IPhysicsFactory
    {
    public:
        CHEngine::IPhysicsWorld* CreateWorld(const CHEngine::PhysicsWorldDesc& worldDesc) override;

        CHEngine::IPhysicsShape* CreateBoxShape(const glm::vec3& halfExtents) override;
        CHEngine::IPhysicsShape* CreateSphereShape(float radius) override;
        CHEngine::IPhysicsShape* CreateCapsuleShape(float radius, float halfHeight) override;

        void Delete(CHEngine::IPhysicsWorld* world) override;
        void Delete(CHEngine::IPhysicsShape* shape) override;

        CHEngine::EPhysicsAPI GetPhysicsApi() override;
        bool CheckIsWorking() override;

        CHEngine::ModuleType GetType() const override;
    };
}
