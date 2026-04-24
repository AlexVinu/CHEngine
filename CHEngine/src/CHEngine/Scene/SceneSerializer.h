#pragma once
#include <string>
#include "Scene.h"
#include <nlohmann/json.hpp>

namespace CHEngine {

// Forward declare RenderResourceManager to avoid heavy include in header
class RenderResourceManager;

// Responsible for scene serialization; It shouldn`t own any fields
struct CHENGINE_API SceneSerializer {
    // Saves scene to .chscene JSON file. Returns true on success.
    bool SaveToFile(Ref<Scene> scene, const std::string& path);

    // Loads scene from .chscene JSON file and returns a ready-to-use scene instance.
    Ref<Scene> LoadFromFile(const std::string& path, RenderResourceManager& resources);

    // ── In-memory snapshot (для Play/Stop режима) ─────────────────────────────
    // Сериализует сцену в JSON-объект (без записи на диск).
    nlohmann::json SerializeToJson(Ref<Scene> scene);

    // Восстанавливает сцену из JSON-снапшота (без чтения с диска)
    // и возвращает готовую сцену.
    Ref<Scene> DeserializeFromJson(const nlohmann::json& data, RenderResourceManager& resources);
};

} // namespace CHEngine
