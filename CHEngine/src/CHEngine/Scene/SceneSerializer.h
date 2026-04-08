#pragma once
#include <string>
#include "Scene.h"
#include <nlohmann/json.hpp>

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

    // ── In-memory snapshot (для Play/Stop режима) ─────────────────────────────
    // Сериализует сцену в JSON-объект (без записи на диск).
    nlohmann::json SerializeToJson();

    // Восстанавливает сцену из JSON-снапшота (без чтения с диска).
    // Аналог LoadFromFile, но источник — память.
    bool DeserializeFromJson(const nlohmann::json& data, RenderResourceManager& resources);

private:
    Scene* m_Scene;
    World* m_World;
};

} // namespace CHEngine
