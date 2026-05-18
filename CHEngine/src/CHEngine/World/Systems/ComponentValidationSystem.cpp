#include "chepch.h"
#include "ComponentValidationSystem.h"

#include "CHEngine/Scene/Entity.h"
#include "CHEngine/World/World.h"

#include "CHEngine/World/WorldEvents.h"

#include <boost/uuid/uuid_io.hpp>

namespace CHEngine {


void ComponentValidationSystem::Run(World& world, DeferredOps& deferred_ops, Timestep)
{
    auto scene = world.GetSceneRef();
    // Rigid body
    scene->ForEach<RigidBody3DComponent>([&](EntityHandle handle, const UUID& uuid, RigidBody3DComponent& rigidBody) {
        Entity* entity = scene->TryGetEntity(handle);
        if (!entity || !entity->HasComponent<TransformComponent>()) {
            CHE_CORE_WARN("ComponentValidationSystem: entity {} has RigidBody3DComponent without TransformComponent", uuid);
			world.GetEvents().Publish<DestroyRigidBodyEvent>(SystemPhase::Simulation, DestroyRigidBodyEvent{ handle });
			deferred_ops.RemoveComponent<RigidBody3DComponent>(handle);
            return;
        }

        rigidBody.ShapeDesc.HalfExtents = glm::max(rigidBody.ShapeDesc.HalfExtents, glm::vec3(0.001f));
        rigidBody.ShapeDesc.Radius = glm::max(rigidBody.ShapeDesc.Radius, 0.001f);
        rigidBody.ShapeDesc.HalfHeight = glm::max(rigidBody.ShapeDesc.HalfHeight, 0.001f);
        rigidBody.BodyDesc.Mass = glm::max(rigidBody.BodyDesc.Mass, 0.001f);
    });

    // UI Objects
    scene->ForEach<UIRectTransformComponent>([&](EntityHandle handle, const UUID& uuid, UIRectTransformComponent& transform)
        {
            if (!transform.CanvasRef.IsValid())
            {
                Entity* entity = scene->TryGetEntity(handle);
                if (entity->HasComponent<UIImageComponent>()) deferred_ops.RemoveComponent<UIImageComponent>(handle);
                if (entity->HasComponent<UITextComponent>()) deferred_ops.RemoveComponent<UITextComponent>(handle);
				if (entity->HasComponent<UIPanelComponent>()) deferred_ops.RemoveComponent<UIPanelComponent>(handle);
				if (entity->HasComponent<UIButtonComponent>()) deferred_ops.RemoveComponent<UIButtonComponent>(handle);
				if (entity->HasComponent<UISliderComponent>()) deferred_ops.RemoveComponent<UISliderComponent>(handle);
            }
        });
}

} // namespace CHEngine
