#include "chepch.h"
#include "PhysicsSystem.h"

#include "CHEngine/Scene/Components.h"
#include "CHEngine/Scene/Entity.h"
#include "CHEngine/World/World.h"
#include "CHEngine/World/WorldEvents.h"
#include "CHEngine/Application.h"
#include "CHEngine/Physics/PhysicsSubsystem.h"


#include <algorithm>
#include <glm/gtc/quaternion.hpp>

namespace CHEngine {

void PhysicsSystem::Run(World& world, DeferredOps& deferred_ops, Timestep dt)
{
    if (dt <= 0.0f)
        return;

    PhysWorldHandle runtimeWorld = world.GetPhysicsRuntimeWorld();
    if (!runtimeWorld.IsValid())
        return;

    auto* factory = Application::Get().Physics()->GetFactory();
    auto scene = world.GetSceneRef();

    // Write block: transform → physics (пропускаем, если ничего не изменилось).
    scene->ForEach<RigidBody3DComponent, TransformComponent>([&](EntityHandle, const UUID&, RigidBody3DComponent& rigidBody, TransformComponent& transformComponent) {
        if (!rigidBody.Body.IsValid())
            return;

        const PhysicsBodyType bodyType = factory->GetBodyType(rigidBody.Body);
        if (!ShouldWriteToPhysics(rigidBody.SyncMode, bodyType))
            return;

        if (!transformComponent.Dirty)
            return;   // fast-path: ничего не менялось с прошлого кадра

        const Transform& transform = transformComponent.ObjectTransform;
        PhysicsTransform physicsTransform{};
        physicsTransform.Position = transform.Position;
        physicsTransform.Rotation = glm::quat(glm::radians(transform.Rotation));

        if (bodyType == PhysicsBodyType::Kinematic)
            factory->SetBodyKinematicTarget(rigidBody.Body, physicsTransform);
        else
            factory->SetBodyTransform(rigidBody.Body, physicsTransform);

        transformComponent.Dirty = false;
    });

    factory->StepSimulation(runtimeWorld, dt);

    // Read block: physics → transform (всегда, физика авторитетна для Dynamic).
    scene->ForEach<RigidBody3DComponent, TransformComponent>([&](EntityHandle, const UUID&, RigidBody3DComponent& rigidBody, TransformComponent& transformComponent) {
        if (!rigidBody.Body.IsValid())
            return;

        const PhysicsBodyType bodyType = factory->GetBodyType(rigidBody.Body);
        if (!ShouldReadFromPhysics(rigidBody.SyncMode, bodyType))
            return;

        const PhysicsTransform physicsTransform = factory->GetBodyTransform(rigidBody.Body);
        transformComponent.ObjectTransform.Position = physicsTransform.Position;
        transformComponent.ObjectTransform.Rotation = glm::degrees(glm::eulerAngles(physicsTransform.Rotation));
        // Физика только что записала позицию — обнуляем dirty, чтобы
        // следующий write-блок не re-uploadил то же значение обратно.
        transformComponent.Dirty = false;
    });
}

void PhysicsSystem::OnBegin(World& world, DeferredOps& deferred_ops)
{
    RebuildPhysicsRuntime(world);
    m_RigidBodyAddedHookHandle       = deferred_ops.SubscribeOnComponentAdded<RigidBody3DComponent>(&PhysicsSystem::CreateRigidBody);
    m_RigidBodyRemovedHookHandle     = deferred_ops.SubscribeOnComponentRemoved<RigidBody3DComponent>(&PhysicsSystem::DestroyRigidBody);
    m_RigidBodyTransferOutHookHandle = deferred_ops.SubscribeOnComponentTransferOut<RigidBody3DComponent>(&PhysicsSystem::OnTransferOut);
    m_RigidBodyTransferInHookHandle  = deferred_ops.SubscribeOnComponentTransferIn<RigidBody3DComponent>(&PhysicsSystem::OnTransferIn);
}

void PhysicsSystem::OnEnd(World& world, DeferredOps& deferred_ops)
{
    if (m_RigidBodyAddedHookHandle.IsValid())
    {
        deferred_ops.Unsubscribe(m_RigidBodyAddedHookHandle);
    }

    if (m_RigidBodyRemovedHookHandle.IsValid())
        deferred_ops.Unsubscribe(m_RigidBodyRemovedHookHandle);

    if (m_RigidBodyTransferOutHookHandle.IsValid())
        deferred_ops.Unsubscribe(m_RigidBodyTransferOutHookHandle);

    if (m_RigidBodyTransferInHookHandle.IsValid())
        deferred_ops.Unsubscribe(m_RigidBodyTransferInHookHandle);

    ClearPhysicsRuntime(world);
}

void PhysicsSystem::OnPhaseDispatch(World& world, DeferredOps& deferred_ops)
{
    world.GetEvents().ConsumePhase<DestroyRigidBodyEvent>(GetPhase(), [&](const DestroyRigidBodyEvent& eventData) {
        DestroyRigidBody(world, eventData.entityHandle);
    });
    world.GetEvents().ConsumePhase<CreateRigidBodyEvent>(GetPhase(), [&](const CreateRigidBodyEvent& eventData) {
        CreateRigidBody(world, eventData.entityHandle);
    });
}

bool PhysicsSystem::ShouldWriteToPhysics(RigidBodySyncMode syncMode, PhysicsBodyType bodyType)
{
    return syncMode == RigidBodySyncMode::WriteToPhysics
        || syncMode == RigidBodySyncMode::ReadWrite
        || (syncMode == RigidBodySyncMode::Auto && bodyType == PhysicsBodyType::Kinematic);
}

bool PhysicsSystem::ShouldReadFromPhysics(RigidBodySyncMode syncMode, PhysicsBodyType bodyType)
{
    return syncMode == RigidBodySyncMode::ReadFromPhysics
        || syncMode == RigidBodySyncMode::ReadWrite
        || (syncMode == RigidBodySyncMode::Auto && bodyType != PhysicsBodyType::Kinematic);
}

void PhysicsSystem::RebuildPhysicsRuntime(World& world)
{
    ClearPhysicsRuntime(world);

    if (!Application::Get().HasPhysics())
        return;

    auto* physics = Application::Get().Physics();
    PhysWorldHandle runtimeWorld = physics->CreateWorld(world.GetPhysicsWorldDesc());
    if (!runtimeWorld.IsValid())
        return;

    world.SetPhysicsRuntimeWorld(runtimeWorld);

    auto* factory = physics->GetFactory();
    auto scene = world.GetSceneRef();

    scene->ForEach<RigidBody3DComponent, TransformComponent>([&](EntityHandle, const UUID&, RigidBody3DComponent& rigidBody, TransformComponent& transformComponent) {
        PhysicsTransform initialTransform{};
        initialTransform.Position = transformComponent.ObjectTransform.Position;
        initialTransform.Rotation = glm::quat(glm::radians(transformComponent.ObjectTransform.Rotation));

        rigidBody.Shape = physics->CreateShape(rigidBody.ShapeDesc);
        rigidBody.Body  = factory->CreateRigidBody(runtimeWorld, rigidBody.BodyDesc, rigidBody.Shape);

        if (rigidBody.Body.IsValid())
        {
            factory->SetBodyTransform(rigidBody.Body, initialTransform);
            transformComponent.Dirty = false;   // физика синхронизирована
        }
    });
}

void PhysicsSystem::ClearPhysicsRuntime(World& world)
{
    PhysWorldHandle runtimeWorld = world.GetPhysicsRuntimeWorld();
    world.SetPhysicsRuntimeWorld({});

    if (!Application::Get().HasPhysics())
        return;

    auto* physics = Application::Get().Physics();
    auto* factory = physics->GetFactory();
    auto scene = world.GetSceneRef();

    if (scene)
    {
        scene->ForEach<RigidBody3DComponent>([&](EntityHandle, const UUID&, RigidBody3DComponent& rigidBody) {
            if (rigidBody.Body.IsValid())
            {
                factory->Delete(rigidBody.Body);
                rigidBody.Body = {};
            }
            if (rigidBody.Shape.IsValid())
            {
                physics->Delete(rigidBody.Shape);
                rigidBody.Shape = {};
            }
        });
    }

    if (runtimeWorld.IsValid())
        physics->DestroyWorld(runtimeWorld);
}

void PhysicsSystem::CreateRigidBody(World& world, EntityHandle handle)
{
    PhysWorldHandle runtimeWorld = world.GetPhysicsRuntimeWorld();
    if (!runtimeWorld.IsValid() || !Application::Get().HasPhysics())
        return;

    auto scene = world.GetSceneRef();
    if (!scene)
        return;

    auto* entity = scene->TryGetEntity(handle);
    if (!entity || !entity->HasComponent<RigidBody3DComponent>())
        return;

    auto& rigidBody = entity->GetComponent<RigidBody3DComponent>();
    auto& transform = entity->GetComponent<TransformComponent>().ObjectTransform;

    PhysicsTransform initialTransform{};
    initialTransform.Position = transform.Position;
    initialTransform.Rotation = glm::quat(glm::radians(transform.Rotation));

    auto* physics = Application::Get().Physics();
    auto* factory = physics->GetFactory();

    rigidBody.Shape = physics->CreateShape(rigidBody.ShapeDesc);
    rigidBody.Body  = factory->CreateRigidBody(runtimeWorld, rigidBody.BodyDesc, rigidBody.Shape);

    if (rigidBody.Body.IsValid())
    {
        factory->SetBodyTransform(rigidBody.Body, initialTransform);
        entity->PatchComponent<TransformComponent>([](TransformComponent& tc) { tc.Dirty = false; });
    }
}

void PhysicsSystem::DestroyRigidBody(World& world, EntityHandle handle)
{
    auto scene = world.GetSceneRef();
    if (!scene || !Application::Get().HasPhysics())
        return;

    auto* entity = scene->TryGetEntity(handle);
    if (!entity || !entity->HasComponent<RigidBody3DComponent>())
        return;

    auto& rigidBody = entity->GetComponent<RigidBody3DComponent>();

    auto* physics = Application::Get().Physics();
    auto* factory = physics->GetFactory();

    if (rigidBody.Body.IsValid())
    {
        factory->Delete(rigidBody.Body);
        rigidBody.Body = {};
    }
    if (rigidBody.Shape.IsValid())
    {
        physics->Delete(rigidBody.Shape);
        rigidBody.Shape = {};
    }
}

void PhysicsSystem::OnTransferOut(World& world, EntityHandle handle, TransferContext& ctx)
{
    auto scene = world.GetSceneRef();
    if (!scene || !Application::Get().HasPhysics()) return;

    auto* entity = scene->TryGetEntity(handle);
    if (!entity || !entity->HasComponent<RigidBody3DComponent>()) return;

    auto& rigidBody = entity->GetComponent<RigidBody3DComponent>();

    auto* physics = Application::Get().Physics();
    auto* factory = physics->GetFactory();

    if (rigidBody.Body.IsValid())
    {
        ctx.Set<glm::vec3>(factory->GetBodyLinearVelocity(rigidBody.Body));
        factory->Delete(rigidBody.Body);
        rigidBody.Body = {};
    }

    if (rigidBody.Shape.IsValid())
    {
        physics->Delete(rigidBody.Shape);
        rigidBody.Shape = {};
    }
}

void PhysicsSystem::OnTransferIn(World& world, EntityHandle handle, const TransferContext& ctx)
{
    PhysWorldHandle runtimeWorld = world.GetPhysicsRuntimeWorld();
    if (!runtimeWorld.IsValid() || !Application::Get().HasPhysics()) return;

    auto scene = world.GetSceneRef();
    if (!scene) return;

    auto* entity = scene->TryGetEntity(handle);
    if (!entity || !entity->HasComponent<RigidBody3DComponent>()) return;

    auto& rigidBody = entity->GetComponent<RigidBody3DComponent>();
    auto& transform = entity->GetComponent<TransformComponent>().ObjectTransform;

    PhysicsTransform physicsTransform{};
    physicsTransform.Position = transform.Position;
    physicsTransform.Rotation = glm::quat(glm::radians(transform.Rotation));

    auto* physics = Application::Get().Physics();
    auto* factory = physics->GetFactory();

    rigidBody.Shape = physics->CreateShape(rigidBody.ShapeDesc);
    rigidBody.Body  = factory->CreateRigidBody(runtimeWorld, rigidBody.BodyDesc, rigidBody.Shape);

    if (rigidBody.Body.IsValid())
    {
        factory->SetBodyTransform(rigidBody.Body, physicsTransform);
        if (const glm::vec3* vel = ctx.TryGet<glm::vec3>())
            factory->SetBodyLinearVelocity(rigidBody.Body, *vel);
    }
}

} // namespace CHEngine
