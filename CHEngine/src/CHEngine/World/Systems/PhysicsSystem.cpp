#include "chepch.h"
#include "PhysicsSystem.h"

#include "CHEngine/Scene/Components.h"
#include "CHEngine/Scene/Entity.h"
#include "CHEngine/World/World.h"
#include "CHEngine/World/WorldEvents.h"
#include "CHEngine/Physics/PhysicsFacade.h"


#include <algorithm>
#include <glm/gtc/quaternion.hpp>

namespace CHEngine {

void PhysicsSystem::Run(World& world, DeferredOps& deferred_ops, Timestep dt)
{
    if (dt <= 0.0f)
        return;

    IPhysicsWorld* runtimeWorld = world.GetPhysicsRuntimeWorld().get();
    if (!runtimeWorld)
        return;

    auto scene = world.GetSceneRef();

    scene->ForEach<RigidBody3DComponent, TransformComponent>([&](EntityHandle, const UUID&, RigidBody3DComponent& rigidBody, TransformComponent& transformComponent) {
        if (!rigidBody.Body)
            return;

        const PhysicsBodyType bodyType = rigidBody.Body->GetType();
        if (!ShouldWriteToPhysics(rigidBody.SyncMode, bodyType))
            return;

        const Transform& transform = transformComponent.ObjectTransform;
        PhysicsTransform physicsTransform{};
        physicsTransform.Position = transform.Position;
        physicsTransform.Rotation = glm::quat(glm::radians(transform.Rotation));

        if (bodyType == PhysicsBodyType::Kinematic)
            rigidBody.Body->SetKinematicTarget(physicsTransform);
        else
            rigidBody.Body->SetTransform(physicsTransform);
    });

    runtimeWorld->StepSimulation(dt);

    scene->ForEach<RigidBody3DComponent, TransformComponent>([&](EntityHandle, const UUID&, RigidBody3DComponent& rigidBody, TransformComponent& transformComponent) {
        if (!rigidBody.Body)
            return;

        const PhysicsBodyType bodyType = rigidBody.Body->GetType();
        if (!ShouldReadFromPhysics(rigidBody.SyncMode, bodyType))
            return;

        const PhysicsTransform physicsTransform = rigidBody.Body->GetTransform();
        transformComponent.ObjectTransform.Position = physicsTransform.Position;
        transformComponent.ObjectTransform.Rotation = glm::degrees(glm::eulerAngles(physicsTransform.Rotation));
    });
}

void PhysicsSystem::OnBegin(World& world, DeferredOps& deferred_ops)
{
    RebuildPhysicsRuntime(world);
    m_RigidBodyAddedHookHandle = deferred_ops.SubscribeOnComponentAdded<RigidBody3DComponent>(&PhysicsSystem::CreateRigidBody);
    m_RigidBodyRemovedHookHandle = deferred_ops.SubscribeOnComponentRemoved<RigidBody3DComponent>(&PhysicsSystem::DestroyRigidBody);
}

void PhysicsSystem::OnEnd(World& world, DeferredOps& deferred_ops)
{
    if (m_RigidBodyAddedHookHandle.IsValid())
    {
        deferred_ops.Unsubscribe(m_RigidBodyAddedHookHandle);
    }

    if (m_RigidBodyRemovedHookHandle.IsValid())
    {
        deferred_ops.Unsubscribe(m_RigidBodyRemovedHookHandle);
    }

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

    IPhysicsWorld* runtimeWorld = PhysicsFacade::CreateWorld(world.GetPhysicsWorldDesc());
    if (!runtimeWorld)
        return;

    world.GetPhysicsRuntimeWorld().reset(runtimeWorld);

    auto scene = world.GetSceneRef();

    scene->ForEach<RigidBody3DComponent, TransformComponent>([&](EntityHandle, const UUID&, RigidBody3DComponent& rigidBody, TransformComponent& transformComponent) {
        PhysicsTransform initialTransform{};
        initialTransform.Position = transformComponent.ObjectTransform.Position;
        initialTransform.Rotation = glm::quat(glm::radians(transformComponent.ObjectTransform.Rotation));

        auto bodyDesc = rigidBody.BodyDesc;
        auto shapeDesc = rigidBody.ShapeDesc;
        auto* shape = PhysicsFacade::CreateShape(shapeDesc);
        rigidBody.Shape = shape;
        rigidBody.Body = world.GetPhysicsRuntimeWorld()->CreateRigidBody(bodyDesc, shape);

        if (rigidBody.Body)
            rigidBody.Body->SetTransform(initialTransform);
        });
}

void PhysicsSystem::ClearPhysicsRuntime(World& world)
{
    // Release ownership before DestroyWorld: the factory deletes the instance;
    // unique_ptr must not delete the same pointer again.
    IPhysicsWorld* runtimeWorld = world.GetPhysicsRuntimeWorld().release();
    auto scene = world.GetSceneRef();
    if (scene)
    {
        scene->ForEach<RigidBody3DComponent>([&](EntityHandle, const UUID&, RigidBody3DComponent& rigidBody) {
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
}

void PhysicsSystem::CreateRigidBody(World& world, EntityHandle handle)
{
    IPhysicsWorld* runtimeWorld = world.GetPhysicsRuntimeWorld().get();
    if (!runtimeWorld)
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

    auto* shape = PhysicsFacade::CreateShape(rigidBody.ShapeDesc);
    rigidBody.Shape = shape;
    rigidBody.Body = runtimeWorld->CreateRigidBody(rigidBody.BodyDesc, shape);

    if (rigidBody.Body)
        rigidBody.Body->SetTransform(initialTransform);
}

void PhysicsSystem::DestroyRigidBody(World& world, EntityHandle handle)
{
    auto scene = world.GetSceneRef();
    if (!scene)
        return;

    auto* entity = scene->TryGetEntity(handle);
    if (!entity || !entity->HasComponent<RigidBody3DComponent>())
        return;

    auto& rigidBody = entity->GetComponent<RigidBody3DComponent>();

    if (world.GetPhysicsRuntimeWorld() && rigidBody.Body)
        world.GetPhysicsRuntimeWorld()->DestroyRigidBody(rigidBody.Body);
    rigidBody.Body = nullptr;

    if (PhysicsFacade::IsAvailable() && rigidBody.Shape)
        PhysicsFacade::Delete(rigidBody.Shape);
    rigidBody.Shape = nullptr;
}
} // namespace CHEngine
