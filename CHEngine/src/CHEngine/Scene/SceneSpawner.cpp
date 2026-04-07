#include "chepch.h"
#include "SceneSpawner.h"

#include "Entity.h"

namespace CHEngine {

EntityHandle SceneSpawner::CreateModelEntity(Scene& scene, const std::string& name,
                                             std::vector<Mesh>&& meshes,
                                             const std::string& sourcePath)
{
    const EntityHandle handle = scene.CreateEntity(name);
    if (Entity* entity = scene.TryGetEntity(handle)) {
        auto& mesh = entity->GetComponent<MeshComponent>();
        mesh.Meshes = std::move(meshes);
        mesh.SourcePath = sourcePath;
    }
    return handle;
}

EntityHandle SceneSpawner::CreateLightEntity(Scene& scene, const std::string& name, LightType type)
{
    const EntityHandle handle = scene.CreateEntity(name);
    if (Entity* entity = scene.TryGetEntity(handle))
        entity->AddComponent<LightComponent>(LightComponent{ Light{ type } });
    return handle;
}

} // namespace CHEngine
