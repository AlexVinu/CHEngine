#pragma once

#include <Core.h>

#include "Physics/IPhysicsFactory.h"

namespace CHEngine
{
    class CHENGINE_API PhysicsFacade
    {
    public:
        static bool Init(IPhysicsFactory* physicsFactory);
        static void Shutdown();

        static bool IsAvailable();

        static IPhysicsWorld* CreateWorld(const PhysicsWorldDesc& worldDesc = {});
        static void DestroyWorld(IPhysicsWorld* world);

        static IPhysicsShape* CreateShape(const CHEngine::PhysicsColliderShapeDesc& shapeDesc);
        static void Delete(IPhysicsShape* shape);
    private:
        PhysicsFacade() = delete;
        ~PhysicsFacade() = delete;
        PhysicsFacade(const PhysicsFacade&) = delete;
        PhysicsFacade& operator=(const PhysicsFacade&) = delete;
        PhysicsFacade(PhysicsFacade&&) = delete;
        PhysicsFacade& operator=(PhysicsFacade&&) = delete;
    };
}
