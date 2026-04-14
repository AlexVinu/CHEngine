#pragma once
#include <string>
#include "Scene.h"
#include <nlohmann/json.hpp>

namespace CHEngine {

// Forward declare RenderResourceManager to avoid heavy include in header
class RenderResourceManager;
class World;

// Responsible for scene serialization; It shouldn`t own any fields
struct CHENGINE_API SceneSerializer {
    // Saves scene to .chscene JSON file. Returns true on success.
    bool SaveToFile(Scene* scene, const std::string& path);

    // Loads scene from .chscene JSON file. Needs RenderResourceManager to re-import meshes.
    // Clears the scene first. Returns true on success.
    bool LoadFromFile(Scene* scene, const std::string& path, RenderResourceManager& resources,
                      World* world = nullptr);

    // ── In-memory snapshot (для Play/Stop режима) ─────────────────────────────
    // Сериализует сцену в JSON-объект (без записи на диск).
    nlohmann::json SerializeToJson(Scene* scene);

    // Восстанавливает сцену из JSON-снапшота (без чтения с диска).
    // Аналог LoadFromFile, но источник — память.
    bool DeserializeFromJson(Scene* scene, const nlohmann::json& data, RenderResourceManager& resources,
                             World* world = nullptr);
};

} // namespace CHEngine
