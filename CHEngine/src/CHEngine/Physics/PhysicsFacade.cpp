#include "chepch.h"

#include "PhysicsFacade.h"

#include "CHEngine/Scene/Scene.h"

#include <algorithm>
#include <vector>

namespace
{
    CHEngine::IPhysicsFactory* g_PhysicsFactory = nullptr;
    std::vector<CHEngine::Scene*> g_RegisteredScenes;
}

namespace CHEngine
{
    bool PhysicsFacade::Init(IPhysicsFactory* physicsFactory)
    {
        Shutdown();
        if (!physicsFactory)
            return false;

        g_PhysicsFactory = physicsFactory;
        return true;
    }

    void PhysicsFacade::Shutdown()
    {
        if (g_PhysicsFactory)
            for (Scene* scene : g_RegisteredScenes)
                if (scene)
                {
                    auto* phys_world = scene->m_PhysicsWorld.release();
                    PhysicsFacade::Delete(phys_world);
                }
                    
        g_RegisteredScenes.clear();
        g_PhysicsFactory = nullptr;
    }

    bool PhysicsFacade::IsAvailable()
    {
        return g_PhysicsFactory != nullptr;
    }

    IPhysicsFactory* PhysicsFacade::GetFactory()
    {
        return g_PhysicsFactory;
    }

    IPhysicsWorld* PhysicsFacade::CreateWorld(const PhysicsWorldDesc& worldDesc)
    {
        return g_PhysicsFactory->CreateWorld(worldDesc);
    }

    void PhysicsFacade::Delete(IPhysicsWorld* world)
    {
        g_PhysicsFactory->Delete(world);
    }

    void PhysicsFacade::StepSimulation(Timestep deltaTime)
    {
        if (deltaTime <= 0.0f)
            return;
        for (Scene* scene : g_RegisteredScenes)
        {
            scene->SyncTransformsToPhysics();

            scene->m_PhysicsWorld->StepSimulation(deltaTime);

            scene->SyncTransformsFromPhysics();
        }
    }

    void PhysicsFacade::RegisterScene(Scene* scene)
    {
        if (!scene)
            return;
        if (std::find(g_RegisteredScenes.begin(), g_RegisteredScenes.end(), scene) != g_RegisteredScenes.end())
            return;

        IPhysicsWorld* world = PhysicsFacade::CreateWorld(scene->m_PhysicsWorldDesc);
        CHE_ASSERT(world, "Physics World wasn`t made");

        scene->m_PhysicsWorld.reset(world);

        g_RegisteredScenes.push_back(scene);
    }

    void PhysicsFacade::UnregisterScene(Scene* scene)
    {
        if (!scene)
            return;

        g_RegisteredScenes.erase(
            std::remove(g_RegisteredScenes.begin(), g_RegisteredScenes.end(), scene),
            g_RegisteredScenes.end());

        auto* phys_world = scene->m_PhysicsWorld.release();
        PhysicsFacade::Delete(phys_world);
    }
}
