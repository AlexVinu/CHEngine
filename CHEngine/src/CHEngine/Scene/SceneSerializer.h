#pragma once
#include <string>
#include "Scene.h"
#include <nlohmann/json.hpp>

namespace CHEngine {

// Responsible for scene serialization; it should not own any fields.
struct CHENGINE_API SceneSerializer {
    // Saves scene to .chscene JSON file. Returns true on success.
    bool SaveToFile(Ref<Scene> scene, const std::string& path);

    // Loads scene from .chscene JSON file and returns a ready-to-use scene instance.
    Ref<Scene> LoadFromFile(const std::string& path);

    // ── In-memory snapshot ───────────────────────────────
    nlohmann::json SerializeToJson(Ref<Scene> scene);

    // Restores scene from JSON snapshot.
    Ref<Scene> DeserializeFromJson(const nlohmann::json& data);
};

} // namespace CHEngine
