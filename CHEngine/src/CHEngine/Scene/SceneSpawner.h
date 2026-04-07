#pragma once

#include "Scene.h"

namespace CHEngine {

class CHENGINE_API SceneSpawner {
public:
    static EntityHandle CreateModelEntity(Scene& scene, const std::string& name,
                                          std::vector<Mesh>&& meshes,
                                          const std::string& sourcePath = "");
    static EntityHandle CreateLightEntity(Scene& scene, const std::string& name, LightType type);
};

} // namespace CHEngine
