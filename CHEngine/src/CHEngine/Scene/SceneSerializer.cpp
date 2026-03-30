#include "chepch.h"
#include "SceneSerializer.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <CHEngine/Mesh/ModelLoader.h>
#include <CHEngine/Render/RenderResourceManager.h>
#include <CHEngine/Scene/Components.h>
#include "Entity.h"
#include <Log/Log.h>
#include <glm/glm.hpp>

using json = nlohmann::json;

namespace CHEngine {

SceneSerializer::SceneSerializer(Scene* scene) : m_Scene(scene) {}

bool SceneSerializer::SaveToFile(const std::string& path) {
    json j;
    j["version"] = 1;
    j["objects"]  = json::array();

    m_Scene->ForEach<TagComponent>([&](EntityHandle handle, TagComponentIDType, TagComponent& tag) {
        auto* transformComp = m_Scene->TryGetComponent<TransformComponent>(handle);
        auto* meshPtr = m_Scene->TryGetComponent<MeshComponent>(handle);
        auto* colorPtr = m_Scene->TryGetComponent<ColorComponent>(handle);
        auto* visibilityPtr = m_Scene->TryGetComponent<VisibilityComponent>(handle);
        if (!transformComp || !meshPtr || !colorPtr || !visibilityPtr)
            return;
        auto& mesh = *meshPtr;
        auto& color = *colorPtr;
        auto& visibility = *visibilityPtr;
        auto& transform = transformComp->ObjectTransform;
        json o;
        o["name"]    = tag.Name;
        o["visible"] = visibility.Visible;
        o["color"]   = { color.Color.r, color.Color.g, color.Color.b, color.Color.a };

        o["position"] = { transform.Position.x, transform.Position.y, transform.Position.z };
        o["rotation"] = { transform.Rotation.x, transform.Rotation.y, transform.Rotation.z };
        o["scale"]    = { transform.Scale.x,    transform.Scale.y,    transform.Scale.z    };

        // Retrieve SourcePath via Scene's accessor (avoids exposing entt publicly)
        o["meshPath"] = mesh.SourcePath;

        // Сериализация материалов по мешам (пути к текстурам + shininess)
        json mats = json::array();
        for (auto& meshItem : mesh.Meshes) {
            json m;
            m["diffusePath"]  = meshItem.Mat.DiffuseMapPath;
            m["specularPath"] = meshItem.Mat.SpecularMapPath;
            m["shininess"]    = meshItem.Mat.Shininess;
            mats.push_back(m);
        }
        o["materials"] = mats;

        // Сериализация источника света
        if (const auto* lightComp = m_Scene->TryGetComponent<LightComponent>(handle)) {
            const auto& lightData = lightComp->LightData;
            json light;
            light["type"]      = static_cast<int>(lightData.Type);
            light["color"]     = { lightData.Color.r, lightData.Color.g, lightData.Color.b };
            light["intensity"] = lightData.Intensity;
            light["range"]     = lightData.Range;
            light["innerCone"] = lightData.InnerCone;
            light["outerCone"] = lightData.OuterCone;
            o["light"] = light;
        }

        j["objects"].push_back(o);
    });

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
    m_Scene->ForEach<MeshComponent>([&](EntityHandle, TagComponentIDType, MeshComponent& meshComp) {
        for (auto& mesh : meshComp.Meshes) {
            if (mesh.GetVertexArray().IsValid())
                resources.DestroyVertexArray(mesh.GetVertexArray());
            if (mesh.Mat.DiffuseMap.IsValid())
                resources.DestroyTexture(mesh.Mat.DiffuseMap);
            if (mesh.Mat.SpecularMap.IsValid())
                resources.DestroyTexture(mesh.Mat.SpecularMap);
        }
    });
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

        EntityHandle handle{};

        if (!meshPath.empty()) {
            // Re-import model from disk
            auto result = ModelLoader::Load(meshPath);
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
                        mesh.Build(verts, mesh.GetIndices());
                    }
                }

                handle = m_Scene->CreateModelEntity(name, std::move(result.meshes), meshPath);
            }
        }

        if (!m_Scene->IsEntityHandleValid(handle))
            handle = m_Scene->CreateEntity(name);
        auto* obj = m_Scene->TryGetEntity(handle);
        if (!obj) continue;

        auto& transform = obj->GetComponent<TransformComponent>().ObjectTransform;
        auto& color = obj->GetComponent<ColorComponent>().Color;
        auto& visible = obj->GetComponent<VisibilityComponent>().Visible;
        auto& meshes = obj->GetComponent<MeshComponent>().Meshes;

        // Restore transform (with validation)
        float v3[3];
        if (o.contains("position") && readFloats(o["position"], 3, v3))
            transform.Position = { v3[0], v3[1], v3[2] };

        if (o.contains("rotation") && readFloats(o["rotation"], 3, v3))
            transform.Rotation = { v3[0], v3[1], v3[2] };

        if (o.contains("scale") && readFloats(o["scale"], 3, v3))
            transform.Scale = { v3[0], v3[1], v3[2] };

        float v4[4];
        if (o.contains("color") && readFloats(o["color"], 4, v4))
            color = { v4[0], v4[1], v4[2], v4[3] };

        visible = o.value("visible", true);

        // Десериализация материалов — перезаписываем поверх того, что загрузил ModelLoader
        if (o.contains("materials") && o["materials"].is_array()) {
            const auto& mats = o["materials"];
            for (size_t mi = 0; mi < meshes.size() && mi < mats.size(); ++mi) {
                const auto& mj = mats[mi];
                auto& mat = meshes[mi].Mat;

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
                CHE_CORE_WARN("SceneSerializer: invalid light type {} for '{}', resetting", typeVal, name);
                typeVal = -1;
            }
            Light lightData;
            lightData.Type = static_cast<LightType>(typeVal);
            float lc[3];
            if (lj.contains("color") && readFloats(lj["color"], 3, lc))
                lightData.Color = { lc[0], lc[1], lc[2] };
            lightData.Intensity = lj.value("intensity", 1.0f);
            lightData.Range     = lj.value("range", 10.0f);
            lightData.InnerCone = lj.value("innerCone", 12.5f);
            lightData.OuterCone = lj.value("outerCone", 17.5f);
            if (obj->HasComponent<LightComponent>())
                obj->RemoveComponent<LightComponent>();
            obj->AddComponent<LightComponent>(LightComponent{ lightData });
        } else if (obj->HasComponent<LightComponent>()) {
            obj->RemoveComponent<LightComponent>();
        }
    }

    CHE_CORE_INFO("Scene loaded: {}", path);
    return true;
}

} // namespace CHEngine
