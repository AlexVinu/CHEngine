#include "chepch.h"
#include "PhysicsSystem.h"

#include "CHEngine/Scene/Components.h"
#include "CHEngine/Scene/Entity.h"
#include "CHEngine/World/World.h"

#include <glm/gtc/quaternion.hpp>

namespace CHEngine {

void PhysicsSystem::run(World& world, CommandBuffer&, Timestep dt)
{
    if (dt <= 0.0f)
        return;

    IPhysicsWorld* runtimeWorld = world.physicsRuntimeWorld().get();
    if (!runtimeWorld)
        return;

    Scene& scene = world.scene();

    scene.ForEach<RigidBody3DComponent>([&](EntityHandle handle, const UUID&, RigidBody3DComponent& rigidBody) {
        if (!rigidBody.Body)
            return;

        Entity* entity = scene.TryGetEntity(handle);
        if (!entity || !entity->HasComponent<TransformComponent>())
            return;
        auto& transformComponent = entity->GetComponent<TransformComponent>();

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

    scene.ForEach<RigidBody3DComponent>([&](EntityHandle handle, const UUID&, RigidBody3DComponent& rigidBody) {
        if (!rigidBody.Body)
            return;

        Entity* entity = scene.TryGetEntity(handle);
        if (!entity || !entity->HasComponent<TransformComponent>())
            return;
        auto& transformComponent = entity->GetComponent<TransformComponent>();

        const PhysicsBodyType bodyType = rigidBody.Body->GetType();
        if (!ShouldReadFromPhysics(rigidBody.SyncMode, bodyType))
            return;

        const PhysicsTransform physicsTransform = rigidBody.Body->GetTransform();
        transformComponent.ObjectTransform.Position = physicsTransform.Position;
        transformComponent.ObjectTransform.Rotation = glm::degrees(glm::eulerAngles(physicsTransform.Rotation));
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

} // namespace CHEngine
