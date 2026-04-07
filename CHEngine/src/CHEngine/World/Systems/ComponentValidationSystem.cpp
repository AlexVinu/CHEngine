#include "chepch.h"
#include "ComponentValidationSystem.h"

#include "CHEngine/Scene/Entity.h"
#include "CHEngine/World/World.h"
#include <boost/uuid/uuid_io.hpp>

namespace CHEngine {

void ComponentValidationSystem::run(World& world, CommandBuffer&, Timestep)
{
    Scene& scene = world.scene();
    std::vector<EntityHandle> rigidBodiesWithoutTransform;
    scene.ForEach<RigidBody3DComponent>([&](EntityHandle handle, const UUID& uuid, RigidBody3DComponent& rigidBody) {
        auto* transform = scene.TryGetComponent<TransformComponent>(handle);
        if (!transform) {
            CHE_CORE_WARN("ComponentValidationSystem: entity {} has RigidBody3DComponent without TransformComponent",
                          boost::uuids::to_string(uuid));
            rigidBodiesWithoutTransform.push_back(handle);
            return;
        }

        rigidBody.ShapeDesc.HalfExtents = glm::max(rigidBody.ShapeDesc.HalfExtents, glm::vec3(0.001f));
        rigidBody.ShapeDesc.Radius = glm::max(rigidBody.ShapeDesc.Radius, 0.001f);
        rigidBody.ShapeDesc.HalfHeight = glm::max(rigidBody.ShapeDesc.HalfHeight, 0.001f);
        rigidBody.BodyDesc.Mass = glm::max(rigidBody.BodyDesc.Mass, 0.001f);
    });

    for (const EntityHandle handle : rigidBodiesWithoutTransform) {
        world.DestroyRigidBodyRuntime(handle);
        if (Entity* entity = scene.TryGetEntity(handle))
            entity->RemoveComponent<RigidBody3DComponent>();
    }
}

} // namespace CHEngine
