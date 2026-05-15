#pragma once

#include <CHEngine.h>
#include <CHEngine/World/World.h>

#include <string>

namespace Sandbox {

/// Uses nlohmann::json via CHEngine::SceneSerializer. No UI or editor subsystems.
class SceneSerializer
{
public:
    /// Serializes \p world.GetScene() to \p path (pretty-printed JSON). Updates recent list on success.
    bool Save(Ref<CHEngine::Scene> editor_scene, const std::string& path);

    /// Loads a scene from \p path. Returns null on failure. Updates recent list on success.
    Ref<CHEngine::Scene> Load(const std::string& path);
};

} // namespace Sandbox
