#include "chepch.h"
#include "SceneSerializer.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <CHEngine/Mesh/ModelLoader.h>
#include <CHEngine/Render/RenderResourceManager.h>
#include <CHEngine/Scene/Components.h>
#include <Log/Log.h>

using json = nlohmann::json;

namespace CHEngine {

SceneSerializer::SceneSerializer(Scene* scene) : m_Scene(scene) {}

bool SceneSerializer::SaveToFile(const std::string& path) {
    json j;
    j["version"] = 1;
    j["objects"]  = json::array();

    for (auto& obj : m_Scene->GetObjects()) {
        json o;
        o["name"]    = obj->Name;
        o["visible"] = obj->Visible;
        o["color"]   = { obj->Color.r, obj->Color.g, obj->Color.b, obj->Color.a };

        auto& t = obj->ObjectTransform;
        o["position"] = { t.Position.x, t.Position.y, t.Position.z };
        o["rotation"] = { t.Rotation.x, t.Rotation.y, t.Rotation.z };
        o["scale"]    = { t.Scale.x,    t.Scale.y,    t.Scale.z    };

        // Retrieve SourcePath via Scene's accessor (avoids exposing entt publicly)
        o["meshPath"] = m_Scene->GetMeshSourcePath(obj->ID);

        // Сериализация источника света
        if (obj->LightData.Type != LightType::None) {
            json light;
            light["type"]      = static_cast<int>(obj->LightData.Type);
            light["color"]     = { obj->LightData.Color.r, obj->LightData.Color.g, obj->LightData.Color.b };
            light["intensity"] = obj->LightData.Intensity;
            light["range"]     = obj->LightData.Range;
            light["innerCone"] = obj->LightData.InnerCone;
            light["outerCone"] = obj->LightData.OuterCone;
            o["light"] = light;
        }

        j["objects"].push_back(o);
    }

    std::ofstream file(path);
    if (!file.is_open()) {
        CHE_CORE_ERROR("SceneSerializer: cannot write to {}", path);
        return false;
    }
    file << j.dump(4);
    CHE_CORE_INFO("Scene saved: {}", path);
    return true;
}

bool SceneSerializer::LoadFromFile(const std::string& path, RenderResourceManager& resources) {
    std::ifstream file(path);
    if (!file.is_open()) {
        CHE_CORE_ERROR("SceneSerializer: cannot read {}", path);
        return false;
    }

    json j;
    try { file >> j; }
    catch (const std::exception& e) {
        CHE_CORE_ERROR("SceneSerializer: JSON parse error: {}", e.what());
        return false;
    }

    // Validate top-level structure
    if (!j.is_object() || !j.contains("objects") || !j["objects"].is_array()) {
        CHE_CORE_ERROR("SceneSerializer: malformed scene file (missing 'objects' array)");
        return false;
    }

    // Clear existing scene GPU resources first
    for (auto& obj : m_Scene->GetObjects()) {
        for (auto& mesh : obj->Meshes) {
            if (mesh.GetVertexArray().IsValid())
                resources.DestroyVertexArray(mesh.GetVertexArray());
            if (mesh.DiffuseTexture.IsValid())
                resources.DestroyTexture(mesh.DiffuseTexture);
        }
    }
    m_Scene->Clear();

    // Helper: safely read a JSON array of N floats
    auto readFloats = [](const json& arr, size_t count, float* out) -> bool {
        if (!arr.is_array() || arr.size() < count) return false;
        for (size_t i = 0; i < count; ++i) {
            if (!arr[i].is_number()) return false;
            out[i] = arr[i].get<float>();
        }
        return true;
    };

    for (auto& o : j["objects"]) {
        if (!o.is_object()) {
            CHE_CORE_WARN("SceneSerializer: skipping non-object entry in 'objects' array");
            continue;
        }

        std::string name     = o.value("name", "Object");
        std::string meshPath = o.value("meshPath", "");

        SceneObject* obj = nullptr;

        if (!meshPath.empty()) {
            // Re-import model from disk
            auto result = ModelLoader::Load(meshPath, resources);
            if (result.success)
                obj = m_Scene->AddModel(name, std::move(result.meshes), meshPath);
        }

        if (!obj)
            obj = m_Scene->AddObject(name);
        if (!obj) continue;

        // Restore transform (with validation)
        float v3[3];
        if (o.contains("position") && readFloats(o["position"], 3, v3))
            obj->ObjectTransform.Position = { v3[0], v3[1], v3[2] };

        if (o.contains("rotation") && readFloats(o["rotation"], 3, v3))
            obj->ObjectTransform.Rotation = { v3[0], v3[1], v3[2] };

        if (o.contains("scale") && readFloats(o["scale"], 3, v3))
            obj->ObjectTransform.Scale = { v3[0], v3[1], v3[2] };

        float v4[4];
        if (o.contains("color") && readFloats(o["color"], 4, v4))
            obj->Color = { v4[0], v4[1], v4[2], v4[3] };

        obj->Visible = o.value("visible", true);

        // Десериализация источника света
        if (o.contains("light") && o["light"].is_object()) {
            auto& lj = o["light"];
            obj->LightData.Type = static_cast<LightType>(lj.value("type", -1));
            float lc[3];
            if (lj.contains("color") && readFloats(lj["color"], 3, lc))
                obj->LightData.Color = { lc[0], lc[1], lc[2] };
            obj->LightData.Intensity = lj.value("intensity", 1.0f);
            obj->LightData.Range     = lj.value("range", 10.0f);
            obj->LightData.InnerCone = lj.value("innerCone", 12.5f);
            obj->LightData.OuterCone = lj.value("outerCone", 17.5f);
        }
    }

    CHE_CORE_INFO("Scene loaded: {}", path);
    return true;
}

} // namespace CHEngine
