#include "chepch.h"
#include "World.h"

#include "Systems/ComponentValidationSystem.h"
#include "Systems/LifetimeSystem.h"
#include "Systems/PhysicsSystem.h"
#include "Systems/RenderSystem.h"
#include "Systems/TransformDirtySystem.h"
#include "CHEngine/Physics/PhysicsFacade.h"

#include <glm/gtc/quaternion.hpp>
#include <vector>

namespace CHEngine
{
    World::World()
    {
        registerDefaultSystems();
    }

    World::World(Scene* scene)
        : m_Scene(scene)
    {
        registerDefaultSystems();
        if (m_Scene)
            RebuildPhysicsRuntime();
    }

    void World::setScene(Scene* scene)
    {
        if (m_Scene == scene)
            return;

        ClearPhysicsRuntime();
        m_Scene = scene;
        m_CommandBuffer.clear();
        if (m_Scene)
            RebuildPhysicsRuntime();
    }

    void World::setPhysicsWorldDesc(const PhysicsWorldDesc& worldDesc)
    {
        m_PhysicsWorldDesc = worldDesc;
        RebuildPhysicsRuntime();
    }

    Scene& World::scene()
    {
        CHE_CORE_ASSERT(m_Scene, "World::scene called without a bound scene");
        return *m_Scene;
    }

    const Scene& World::scene() const
    {
        CHE_CORE_ASSERT(m_Scene, "World::scene called without a bound scene");
        return *m_Scene;
    }

    void World::update(Timestep dt)
    {
        if (!m_Scene)
            return;

        if (m_Simulating)
            m_Scheduler.runPhase(SystemPhase::Simulation, *this, m_CommandBuffer, dt);

        if (m_Active)
            m_Scheduler.runPhase(SystemPhase::Presentation, *this, m_CommandBuffer, dt);

        std::vector<EntityHandle> pendingDestroyHandles;
        pendingDestroyHandles.reserve(m_CommandBuffer.size());
        m_CommandBuffer.collectPendingDestroyHandles(*m_Scene, pendingDestroyHandles);
        for (const EntityHandle handle : pendingDestroyHandles)
            DestroyRigidBodyRuntime(handle);

        m_CommandBuffer.flush(*m_Scene);
    }

    void World::OnEvent(Event& event)
    {
        if (!m_Scene)
            return;
        m_Scheduler.dispatchEvent(event, *this, m_CommandBuffer);
    }

    void World::RebuildPhysicsRuntime()
    {
        ClearPhysicsRuntime();
        if (!m_Scene)
            return;

        IPhysicsWorld* runtimeWorld = PhysicsFacade::CreateWorld(m_PhysicsWorldDesc);
        if (!runtimeWorld)
            return;
        m_PhysicsRuntimeWorld.reset(runtimeWorld);

        m_Scene->ForEach<RigidBody3DComponent>([&](EntityHandle handle, const UUID&, RigidBody3DComponent& rigidBody) {
            auto* transformComponent = m_Scene->TryGetComponent<TransformComponent>(handle);
            if (!transformComponent)
                return;

            PhysicsTransform initialTransform{};
            initialTransform.Position = transformComponent->ObjectTransform.Position;
            initialTransform.Rotation = glm::quat(glm::radians(transformComponent->ObjectTransform.Rotation));
            PhysicsFacade::CreateRigidBodyRuntime(m_PhysicsRuntimeWorld.get(), rigidBody, initialTransform);
        });
    }

    void World::ClearPhysicsRuntime()
    {
        IPhysicsWorld* runtimeWorld = m_PhysicsRuntimeWorld.get();
        if (m_Scene)
        {
            m_Scene->ForEach<RigidBody3DComponent>([&](EntityHandle, const UUID&, RigidBody3DComponent& rigidBody) {
                PhysicsFacade::DestroyRigidBodyRuntime(runtimeWorld, rigidBody);
            });
        }

        if (runtimeWorld)
            PhysicsFacade::DestroyWorld(runtimeWorld);
        m_PhysicsRuntimeWorld.reset(runtimeWorld);
    }

    void World::DestroyRigidBodyRuntime(EntityHandle handle)
    {
        if (!m_Scene)
            return;

        if (auto* rigidBody = m_Scene->TryGetComponent<RigidBody3DComponent>(handle))
            PhysicsFacade::DestroyRigidBodyRuntime(m_PhysicsRuntimeWorld.get(), *rigidBody);
    }

    void World::registerDefaultSystems()
    {
        //m_Scheduler.emplaceSystem<TransformDirtySystem>();
        m_Scheduler.emplaceSystem<LifetimeSystem>();
        m_Scheduler.emplaceSystem<ComponentValidationSystem>();
        m_Scheduler.emplaceSystem<PhysicsSystem>();
        m_Scheduler.emplaceSystem<RenderSystem>();
    }
}
