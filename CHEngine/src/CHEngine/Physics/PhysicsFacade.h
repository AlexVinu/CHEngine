#pragma once

#include <Core.h>

#include "Physics/IPhysicsFactory.h"

namespace CHEngine
{
    class Scene;

    class CHENGINE_API PhysicsFacade
    {
    public:
        static bool Init(IPhysicsFactory* physicsFactory);
        static void Shutdown();

        static bool IsAvailable();
        static IPhysicsFactory* GetFactory();

        static IPhysicsWorld* CreateWorld(const PhysicsWorldDesc& worldDesc = {});
        static void Delete(IPhysicsWorld* world);
        static void StepSimulation(Timestep deltaTime);
        static void RegisterScene(Scene* scene);
        static void UnregisterScene(Scene* scene);

    private:
        PhysicsFacade() = delete;
        ~PhysicsFacade() = delete;
        PhysicsFacade(const PhysicsFacade&) = delete;
        PhysicsFacade& operator=(const PhysicsFacade&) = delete;
        PhysicsFacade(PhysicsFacade&&) = delete;
        PhysicsFacade& operator=(PhysicsFacade&&) = delete;
    };
}
