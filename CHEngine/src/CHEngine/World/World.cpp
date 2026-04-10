#include "chepch.h"
#include "World.h"

#include "Systems/ComponentValidationSystem.h"
#include "Systems/LifetimeSystem.h"
#include "Systems/PhysicsSystem.h"
#include "Systems/RenderSystem.h"
#include "Systems/TransformDirtySystem.h"
#include "CHEngine/Physics/PhysicsFacade.h"
#include "CHEngine/Scene/Entity.h"

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
        m_PhysicsWorld.reset(runtimeWorld);

        m_Scene->ForEach<RigidBody3DComponent>([&](EntityHandle handle, const UUID&, RigidBody3DComponent& rigidBody) {
            auto* entity = m_Scene->TryGetEntity(handle);
            if (!entity || !entity->HasComponent<TransformComponent>())
                return;
            auto& transformComponent = entity->GetComponent<TransformComponent>();

            PhysicsTransform initialTransform{};
            initialTransform.Position = transformComponent.ObjectTransform.Position;
            initialTransform.Rotation = glm::quat(glm::radians(transformComponent.ObjectTransform.Rotation));
            
            auto bodyDesc = rigidBody.BodyDesc;
            auto shapeDesc = rigidBody.ShapeDesc;
            auto* shape = PhysicsFacade::CreateShape(shapeDesc);
            rigidBody.Shape = shape;
            rigidBody.Body = m_PhysicsWorld->CreateRigidBody(bodyDesc, shape);

            if (rigidBody.Body)
                rigidBody.Body->SetTransform(initialTransform);
        });
    }

    void World::ClearPhysicsRuntime()
    {
        IPhysicsWorld* runtimeWorld = m_PhysicsWorld.get();
        if (m_Scene)
        {
            m_Scene->ForEach<RigidBody3DComponent>([&](EntityHandle, const UUID&, RigidBody3DComponent& rigidBody) {
                if (runtimeWorld && rigidBody.Body)
                    runtimeWorld->DestroyRigidBody(rigidBody.Body);
                rigidBody.Body = nullptr;

                if (PhysicsFacade::IsAvailable() && rigidBody.Shape)
                    PhysicsFacade::Delete(rigidBody.Shape);
                rigidBody.Shape = nullptr;

            });
        }

        if (runtimeWorld)
            PhysicsFacade::DestroyWorld(runtimeWorld);
        m_PhysicsWorld.reset(runtimeWorld);
    }

    void World::DestroyRigidBodyRuntime(EntityHandle handle)
    {
        if (!m_Scene)
            return;

        auto* entity = m_Scene->TryGetEntity(handle);
        if (!entity || !entity->HasComponent<RigidBody3DComponent>())
            return;

        auto& rigidBody = entity->GetComponent<RigidBody3DComponent>();

        if (m_PhysicsWorld && rigidBody.Body)
            m_PhysicsWorld->DestroyRigidBody(rigidBody.Body);
        rigidBody.Body = nullptr;

        if (PhysicsFacade::IsAvailable() && rigidBody.Shape)
            PhysicsFacade::Delete(rigidBody.Shape);
        rigidBody.Shape = nullptr;
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
