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

    auto& reg = m_Scene->GetRegistry();

    for (auto& obj : m_Scene->GetObjects()) {
        json o;
        o["name"]    = obj->Name;
        o["visible"] = obj->Visible;
        o["color"]   = { obj->Color.r, obj->Color.g, obj->Color.b, obj->Color.a };

        auto& t = obj->ObjectTransform;
        o["position"] = { t.Position.x, t.Position.y, t.Position.z };
        o["rotation"] = { t.Rotation.x, t.Rotation.y, t.Rotation.z };
        o["scale"]    = { t.Scale.x,    t.Scale.y,    t.Scale.z    };

        // Find SourcePath from ECS MeshComponent by matching TagComponent ID
        std::string srcPath;
        auto view = reg.view<TagComponent, MeshComponent>();
        for (auto e : view) {
            auto& tc = view.get<TagComponent>(e);
            if (tc.ID == obj->ID) {
                srcPath = view.get<MeshComponent>(e).SourcePath;
                break;
            }
        }
        o["meshPath"] = srcPath;

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

    for (auto& o : j["objects"]) {
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

        // Restore transform
        if (o.contains("position")) {
            auto& p = o["position"];
            obj->ObjectTransform.Position = { p[0].get<float>(), p[1].get<float>(), p[2].get<float>() };
        }
        if (o.contains("rotation")) {
            auto& r = o["rotation"];
            obj->ObjectTransform.Rotation = { r[0].get<float>(), r[1].get<float>(), r[2].get<float>() };
        }
        if (o.contains("scale")) {
            auto& s = o["scale"];
            obj->ObjectTransform.Scale = { s[0].get<float>(), s[1].get<float>(), s[2].get<float>() };
        }
        if (o.contains("color")) {
            auto& c = o["color"];
            obj->Color = { c[0].get<float>(), c[1].get<float>(), c[2].get<float>(), c[3].get<float>() };
        }
        obj->Visible = o.value("visible", true);
    }

    CHE_CORE_INFO("Scene loaded: {}", path);
    return true;
}

} // namespace CHEngine
