#pragma once

#include <Core.h>

#include "IModuleFactory.h"
#include "IPhysicsWorld.h"

namespace CHEngine
{
    enum class EPhysicsAPI : uint8_t
    {
        PhysX = 0
    };

    struct IPhysicsFactory : IModuleFactory
    {
        virtual ~IPhysicsFactory() = default;

        virtual IPhysicsWorld* CreateWorld(const PhysicsWorldDesc& worldDesc) = 0;

        virtual IPhysicsShape* CreateBoxShape(const glm::vec3& halfExtents) = 0;
        virtual IPhysicsShape* CreateSphereShape(float radius) = 0;
        virtual IPhysicsShape* CreateCapsuleShape(float radius, float halfHeight) = 0;

        virtual void Delete(IPhysicsWorld* world) = 0;
        virtual void Delete(IPhysicsShape* shape) = 0;

        virtual EPhysicsAPI GetPhysicsApi() = 0;
        virtual bool CheckIsWorking() = 0;
    };
}

DECLARE_MODULE_FACTORY()
