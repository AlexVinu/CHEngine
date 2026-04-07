#pragma once
#include <string>
#include "Scene.h"

namespace CHEngine {

// Forward declare RenderResourceManager to avoid heavy include in header
class RenderResourceManager;
class World;

class CHENGINE_API SceneSerializer {
public:
    explicit SceneSerializer(Scene* scene, World* world = nullptr);

    // Saves scene to .chscene JSON file. Returns true on success.
    bool SaveToFile(const std::string& path);

    // Loads scene from .chscene JSON file. Needs RenderResourceManager to re-import meshes.
    // Clears the scene first. Returns true on success.
    bool LoadFromFile(const std::string& path, RenderResourceManager& resources);

private:
    Scene* m_Scene;
    World* m_World;
};

} // namespace CHEngine
