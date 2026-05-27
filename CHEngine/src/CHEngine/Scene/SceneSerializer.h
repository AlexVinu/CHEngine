#pragma once
#include <string>
#include <filesystem>
#include "Scene.h"
#include <nlohmann/json.hpp>

namespace CHEngine {

// Responsible for scene serialization; it should not own any fields.
struct CHENGINE_API SceneSerializer {
    // Saves scene to .chscene JSON file. Returns true on success.
    bool SaveToFile(Ref<Scene> scene, const std::string& path);

    // Loads scene from .chscene JSON file and returns a ready-to-use scene instance.
    // basePath: project root used to resolve relative SourcePaths (e.g. "Assets/Models/…").
    // Pass empty to fall back to absolute-only behaviour.
    Ref<Scene> LoadFromFile(const std::string& path,
                             const std::filesystem::path& basePath = {});

    // ── In-memory snapshot ───────────────────────────────
    // Returns the scene as a JSON object (callers can add extra fields before writing).
    nlohmann::json BuildJson(Ref<Scene> scene);

    // Convenience: BuildJson + dump(4).
    std::string SerializeToJson(Ref<Scene> scene);

    // Restores scene from JSON snapshot.
    // basePath: project root used to resolve relative SourcePaths. Pass empty to
    // keep the old absolute-only behaviour.
    Ref<Scene> DeserializeFromJson(const nlohmann::json& data,
                                    const std::filesystem::path& basePath = {});
};

} // namespace CHEngine
