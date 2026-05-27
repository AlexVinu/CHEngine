#include "chepch.h"
#include "SceneSerializer.h"

#include "FileSystem/FileSystem.h"
#include "MetaSerializer.h"

#include <nlohmann/json.hpp>
#include <magic_enum/magic_enum.hpp>
#include <cstddef>
#include <memory>
#include <unordered_set>
#include <CHEngine/Mesh/Material.h>
#include <CHEngine/ResourceManager/ResourceManager.h>
#include <CHEngine/Application.h>

#include <CHEngine/Scene/Components.h>
#include "Entity.h"
#include <Log/Log.h>
#include <glm/glm.hpp>
#include <filesystem>

using json = nlohmann::json;
using namespace entt::literals;

namespace CHEngine {

json SerializeScripts(const std::vector<ScriptEntry>& scripts)
{
    json arr = json::array();
    for (const auto& entry : scripts)
    {
        json e;
        e["path"]    = entry.Path;
        e["enabled"] = entry.Enabled;
        arr.push_back(std::move(e));
    }
    return arr;
}

// Reads scripts from object `o`. Supports new "scripts": [...] and legacy
// "script": {"path": "...", "enabled": ...} (single entry). New format wins
// if both are present.
std::vector<ScriptEntry> DeserializeScripts(const json& o)
{
    std::vector<ScriptEntry> result;
    if (o.contains("scripts") && o["scripts"].is_array())
    {
        for (const auto& e : o["scripts"])
        {
            if (!e.is_object()) continue;
            std::string path = e.value("path", "");
            if (path.empty()) continue;
            result.push_back(ScriptEntry{ std::move(path), e.value("enabled", true) });
        }
        if (o.contains("script"))
            CHE_CORE_WARN("SceneSerializer: both 'scripts' and 'script' present; using 'scripts'");
        return result;
    }
    if (o.contains("script") && o["script"].is_object())
    {
        const auto& sj = o["script"];
        std::string path = sj.value("path", "");
        if (!path.empty())
            result.push_back(ScriptEntry{ std::move(path), sj.value("enabled", true) });
    }
    return result;
}

static void SerializePools(json& j, const Ref<Scene>& scene)
{
    // string_pool: sorted by ID for deterministic output
    {
        const auto& pool = scene->GetStringPool();
        if (!pool.empty())
        {
            std::vector<std::pair<StringID, std::string>> sorted(pool.begin(), pool.end());
            std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
            json arr = json::array();
            for (const auto& [id, str] : sorted)
                arr.push_back({ {"id", id}, {"str", str} });
            j["string_pool"] = std::move(arr);
        }
    }
    // script_pool: sorted by ID
    {
        const auto& pool = scene->GetScriptPool();
        if (!pool.empty())
        {
            std::vector<std::pair<ScriptsID, std::vector<ScriptEntry>>> sorted(pool.begin(), pool.end());
            std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
            json arr = json::array();
            for (const auto& [id, scripts] : sorted)
            {
                json e;
                e["id"]      = id;
                e["scripts"] = SerializeScripts(scripts);
                arr.push_back(std::move(e));
            }
            j["script_pool"] = std::move(arr);
        }
    }
}

// ── In-memory snapshot ────────────────────────────────────────────────────────

nlohmann::json SceneSerializer::BuildJson(Ref<Scene> scene) {
    json sceneJson = json::object();
    json entitiesJson = json::array();

    auto& reg = scene->GetRegistryRef();
    for (auto entity : reg.storage<entt::entity>()) {
        json entityJson;
        entityJson["entity_id"] = (uint32_t)entity;

        for (auto [id, type] : entt::resolve()) {
            if (auto serializeFunc = type.func("serialize"_hs)) {
                serializeFunc.invoke({},
                    entt::forward_as_meta(const_cast<const entt::registry&>(reg)),
                    entt::forward_as_meta(entity),
                    entt::forward_as_meta(entityJson));
            }
        }

        if (entityJson.contains("components")) {
            entitiesJson.push_back(std::move(entityJson));
        }
    }

    sceneJson["entities"] = std::move(entitiesJson);

    SerializePools(sceneJson, scene);

    // WorldScripts → an extra script_pool record (non-mutating: pick an ID above any existing one).
    if (!scene->WorldScripts.empty()) {
        ScriptsID maxId = 0;
        for (const auto& [id, _] : scene->GetScriptPool())
            maxId = std::max(maxId, id);
        const ScriptsID worldScriptsId = maxId + 1;

        if (!sceneJson.contains("script_pool"))
            sceneJson["script_pool"] = json::array();
        json e;
        e["id"]      = worldScriptsId;
        e["scripts"] = SerializeScripts(scene->WorldScripts);
        sceneJson["script_pool"].push_back(std::move(e));

        sceneJson["meta"]["world_scripts_id"] = worldScriptsId;
    }

    return sceneJson;
}

std::string SceneSerializer::SerializeToJson(Ref<Scene> scene) {
    return BuildJson(scene).dump(4);
}

Ref<Scene> SceneSerializer::DeserializeFromJson(const nlohmann::json& sceneJson,
                                                  const std::filesystem::path& basePath) {
    Ref<Scene> loadedScene = MakeRef<Scene>();

    // Restore pools before entities so component StringID/ScriptsID values resolve.
    if (sceneJson.contains("string_pool") && sceneJson["string_pool"].is_array()) {
        for (const auto& e : sceneJson["string_pool"]) {
            if (!e.is_object() || !e.contains("id") || !e.contains("str")) continue;
            loadedScene->LoadStringPoolEntry(e["id"].get<StringID>(), e["str"].get<std::string>());
        }
    }
    if (sceneJson.contains("script_pool") && sceneJson["script_pool"].is_array()) {
        for (const auto& e : sceneJson["script_pool"]) {
            if (!e.is_object() || !e.contains("id")) continue;
            loadedScene->LoadScriptPoolEntry(e["id"].get<ScriptsID>(), DeserializeScripts(e));
        }
    }

    // Restore WorldScripts from the referenced pool entry.
    if (sceneJson.contains("meta") && sceneJson["meta"].contains("world_scripts_id")) {
        ScriptsID id = sceneJson["meta"]["world_scripts_id"].get<ScriptsID>();
        if (auto* scripts = loadedScene->GetScripts(id))
            loadedScene->WorldScripts = *scripts;
    }

    const json* entitiesJson = nullptr;
    if (sceneJson.contains("entities") && sceneJson["entities"].is_array())
        entitiesJson = &sceneJson["entities"];
    else if (sceneJson.is_array())
        entitiesJson = &sceneJson; // tolerate the legacy top-level-array form

    if (entitiesJson) {
        // Type names used as JSON keys (resolved once after RegisterAllComponents).
        static const std::string kIDKey  = std::string(entt::resolve<IDComponent>().info().name());
        static const std::string kTagKey = std::string(entt::resolve<TagComponent>().info().name());

        // Pass 1: register every entity in Scene infrastructure (EntityHandlersStore etc.)
        // so ForEach<Components...> can iterate them.
        for (const auto& entityJson : *entitiesJson) {
            UUID        uuid{};
            std::string name = "Object";
            if (entityJson.contains("components")) {
                const auto& comps = entityJson["components"];
                if (comps.contains(kIDKey))
                    uuid = comps[kIDKey].get<IDComponent>().Value;
                if (comps.contains(kTagKey)) {
                    StringID sid = comps[kTagKey].get<TagComponent>().Name;
                    const std::string& s = loadedScene->GetString(sid);
                    if (!s.empty()) name = s;
                }
            }
            loadedScene->CreateEntity(name, uuid);
        }

        // Pass 2: deserialize all components onto the now-registered entities.
        auto& reg = loadedScene->GetRegistryRef();
        // Build UUID → entt::entity lookup for pass 2.
        std::unordered_map<std::string, entt::entity> uuidToEntt;
        for (auto e : reg.view<IDComponent>())
            uuidToEntt[reg.get<IDComponent>(e).Value.ToString()] = e;

        for (const auto& entityJson : *entitiesJson) {
            UUID uuid{};
            if (entityJson.contains("components")) {
                const auto& comps = entityJson["components"];
                if (comps.contains(kIDKey))
                    uuid = comps[kIDKey].get<IDComponent>().Value;
            }
            auto it = uuidToEntt.find(uuid.ToString());
            if (it == uuidToEntt.end()) continue;
            entt::entity entity = it->second;

            for (auto [id, type] : entt::resolve()) {
                if (auto deserializeFunc = type.func("deserialize"_hs)) {
                    deserializeFunc.invoke({},
                        entt::forward_as_meta(reg),
                        entt::forward_as_meta(entity),
                        entt::forward_as_meta(const_cast<const json&>(entityJson)));
                }
            }
        }
    }

    // Reload mesh assets from SourcePath (StringPool → file path).
    // MeshComponent::Mesh is not serialized — it must be rebuilt from SourcePath.
    loadedScene->ForEach<MeshComponent>(
        [&](EntityHandle handle, const UUID&, MeshComponent& meshComp) {
            if (meshComp.Mesh.IsValid()) return;
            const std::string& meshPath = loadedScene->GetString(meshComp.SourcePath);
            if (meshPath.empty()) return;

            MeshLoader* meshLoader = Application::Get().Resources().GetMeshLoader();
            MeshRef importedMesh;
            if (ResourceManager::IsPrimitiveUri(meshPath)) {
                importedMesh = Application::Get().Resources().LoadPrimitiveMesh(meshPath);
                if (importedMesh.IsValid()) {
                    ShaderHandle sh = Application::Get().Render().GetDefaultMeshShader();
                    if (meshPath == ":primitive:sphere") {
                        ShaderHandle sh2 = Application::Get().Render().GetDefaultSphereImpostorShader();
                        if (sh2.IsValid()) sh = sh2;
                    }
                    for (uint32_t i = 0; i < importedMesh->GetSubMeshCount(); ++i)
                        meshLoader->SetMaterial(importedMesh.Handle(), i,
                            MaterialInstance::FromBase(std::make_shared<Material>(sh)));
                }
            } else {
                std::filesystem::path fsPath(meshPath);
                if (!fsPath.is_absolute()) {
                    if (!basePath.empty())
                        fsPath = basePath / fsPath;
                    else {
                        CHE_CORE_WARN("SceneSerializer: SourcePath '{}' is relative but no basePath provided — skipping mesh reload", meshPath);
                        return;
                    }
                }
                auto modelHandle = Application::Get().Resources().Load<ModelHandle>(
                    fsPath, Application::Get().Render().GetDefaultMeshShader());
                if (modelHandle.IsValid()) {
                    const LoadedModel* result = Application::Get().Resources().GetModel(modelHandle);
                    if (result && result->mesh.IsValid())
                        importedMesh = result->mesh;
                }
            }
            if (importedMesh.IsValid())
                meshComp.Mesh = std::move(importedMesh);
            else
                CHE_CORE_WARN("SceneSerializer: failed to reload mesh '{}'", meshPath);
        });

    return loadedScene;
}

bool SceneSerializer::SaveToFile(Ref<Scene> scene, const std::string& path) {
    if (!scene) return false;
    return FileSystem::WriteFileText(path, SerializeToJson(scene));
}

Ref<Scene> SceneSerializer::LoadFromFile(const std::string& path,
                                          const std::filesystem::path& basePath) {
    const std::string txt = FileSystem::ReadFileText(path);
    if (txt.empty()) {
        CHE_CORE_ERROR("SceneSerializer::LoadFromFile: empty or missing file '{}'", path);
        return nullptr;
    }
    try {
        return DeserializeFromJson(json::parse(txt), basePath);
    } catch (const std::exception& e) {
        CHE_CORE_ERROR("SceneSerializer::LoadFromFile: parse error in '{}': {}", path, e.what());
        return nullptr;
    }
}

} // namespace CHEngine
