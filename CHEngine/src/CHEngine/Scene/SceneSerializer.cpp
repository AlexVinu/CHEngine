#include "chepch.h"
#include "SceneSerializer.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <CHEngine/Mesh/ModelLoader.h>
#include <CHEngine/Render/RenderResourceManager.h>
#include <CHEngine/Scene/Components.h>
#include <Log/Log.h>
#include <glm/glm.hpp>

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

        // Сериализация материалов по мешам (пути к текстурам + shininess)
        json mats = json::array();
        for (auto& mesh : obj->Meshes) {
            json m;
            m["diffusePath"]  = mesh.Mat.DiffuseMapPath;
            m["specularPath"] = mesh.Mat.SpecularMapPath;
            m["shininess"]    = mesh.Mat.Shininess;
            mats.push_back(m);
        }
        o["materials"] = mats;

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
            if (mesh.Mat.DiffuseMap.IsValid())
                resources.DestroyTexture(mesh.Mat.DiffuseMap);
            if (mesh.Mat.SpecularMap.IsValid())
                resources.DestroyTexture(mesh.Mat.SpecularMap);
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
            if (result.success) {
                // Центрируем вершины вокруг геометрического центра — точно как ImportModel.
                // Без этого позиция модели смещается на величину centroid при каждой загрузке.
                glm::vec3 centroid(0.0f);
                size_t totalVerts = 0;
                for (auto& mesh : result.meshes) {
                    for (const auto& v : mesh.GetVertices()) {
                        centroid += v.Position;
                        ++totalVerts;
                    }
                }
                if (totalVerts > 0) centroid /= static_cast<float>(totalVerts);

                if (totalVerts > 0 && glm::length(centroid) > 1e-5f) {
                    for (auto& mesh : result.meshes) {
                        resources.DestroyVertexArray(mesh.GetVertexArray());
                        auto verts = mesh.GetVertices();
                        for (auto& v : verts) v.Position -= centroid;
                        mesh.Build(resources, verts, mesh.GetIndices());
                    }
                }

                obj = m_Scene->AddModel(name, std::move(result.meshes), meshPath);
            }
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

        // Десериализация материалов — перезаписываем поверх того, что загрузил ModelLoader
        if (o.contains("materials") && o["materials"].is_array()) {
            const auto& mats = o["materials"];
            for (size_t mi = 0; mi < obj->Meshes.size() && mi < mats.size(); ++mi) {
                const auto& mj = mats[mi];
                auto& mat = obj->Meshes[mi].Mat;

                mat.Shininess = mj.value("shininess", 32.0f);

                std::string diffPath = mj.value("diffusePath", "");
                if (!diffPath.empty() && diffPath != mat.DiffuseMapPath) {
                    if (mat.DiffuseMap.IsValid())
                        resources.DestroyTexture(mat.DiffuseMap);
                    mat.DiffuseMap = resources.CreateTextureFromFile(diffPath);
                    mat.DiffuseMapPath = mat.DiffuseMap.IsValid() ? diffPath : "";
                }

                std::string specPath = mj.value("specularPath", "");
                if (!specPath.empty() && specPath != mat.SpecularMapPath) {
                    if (mat.SpecularMap.IsValid())
                        resources.DestroyTexture(mat.SpecularMap);
                    mat.SpecularMap = resources.CreateTextureFromFile(specPath);
                    mat.SpecularMapPath = mat.SpecularMap.IsValid() ? specPath : "";
                }
            }
        }

        // Десериализация источника света
        if (o.contains("light") && o["light"].is_object()) {
            const auto& lj = o["light"];
            int typeVal = lj.value("type", -1);
            if (typeVal < 0 || typeVal > 2) {
                CHE_CORE_WARN("SceneSerializer: некорректный тип света {} у '{}', сброс", typeVal, name);
                typeVal = -1;
            }
            obj->LightData.Type = static_cast<LightType>(typeVal);
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
