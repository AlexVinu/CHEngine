#include "chepch.h"
#include "SceneSerializer.h"

#include "FileSystem/FileSystem.h"

#include <nlohmann/json.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cstddef>
#include <memory>
#include <unordered_set>
#include <CHEngine/Mesh/Material.h>
#include <CHEngine/Mesh/ModelLoader.h>
#include <CHEngine/Render/RenderFacade.h>
#include <CHEngine/Render/RenderResourceManager.h>
#include <CHEngine/Scene/Components.h>
#include <CHEngine/World/World.h>
#include "Entity.h"
#include <Log/Log.h>
#include <glm/glm.hpp>

using json = nlohmann::json;

namespace CHEngine {

namespace {
constexpr int kSceneFormatVersion = 3;

int HexToNibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

bool TryParseUUIDString(const std::string& input, UUID& outUUID)
{
    std::string s = input;
    if (s.size() == 38 && s.front() == '{' && s.back() == '}')
        s = s.substr(1, s.size() - 2);

    std::string hex;
    hex.reserve(32);
    for (char c : s)
    {
        if (c == '-')
            continue;
        hex.push_back(c);
    }

    if (hex.size() != 32)
        return false;

    UUID parsed = boost::uuids::nil_uuid();
    for (size_t i = 0; i < 16; ++i)
    {
        const int hi = HexToNibble(hex[i * 2]);
        const int lo = HexToNibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0)
            return false;
        *(parsed.begin() + static_cast<std::ptrdiff_t>(i)) = static_cast<uint8_t>((hi << 4) | lo);
    }

    outUUID = parsed;
    return true;
}

static void DestroyTexturesForInstance(MaterialInstance* p, RenderResourceManager& resources,
                                       std::unordered_set<Material*>& baseDiffuseDestroyed,
                                       std::unordered_set<Material*>& baseSpecDestroyed)
{
    TextureHandle d, s;
    p->ResolveTextures(d, s);

    if (d.IsValid())
    {
        if (p->DiffuseMap.IsValid())
            resources.DestroyTexture(d);
        else if (p->m_Material && p->m_Material->DiffuseMap.IsValid())
        {
            if (baseDiffuseDestroyed.insert(p->m_Material.get()).second)
                resources.DestroyTexture(d);
        }
    }
    if (s.IsValid())
    {
        if (p->SpecularMap.IsValid())
            resources.DestroyTexture(s);
        else if (p->m_Material && p->m_Material->SpecularMap.IsValid())
        {
            if (baseSpecDestroyed.insert(p->m_Material.get()).second)
                resources.DestroyTexture(s);
        }
    }
}

void DestroyUniqueMeshTextures(MeshComponent& meshComp, RenderResourceManager& resources)
{
    std::unordered_set<MaterialInstance*> seenInst;
    std::unordered_set<Material*> baseDiffuseDestroyed;
    std::unordered_set<Material*> baseSpecDestroyed;

    for (auto& mesh : meshComp.Meshes)
    {
        if (!mesh.Mat)
            continue;
        MaterialInstance* p = mesh.Mat.get();
        if (!seenInst.insert(p).second)
            continue;
        DestroyTexturesForInstance(p, resources, baseDiffuseDestroyed, baseSpecDestroyed);
    }
}

void ApplyMaterialFromJson(const json& mj, MaterialInstance& mat, RenderResourceManager& resources)
{
    mat.Shininess     = mj.value("shininess", 32.0f);
    mat.SpecularScale = mj.value("specularScale", 1.0f);

    std::string diffPath = mj.value("diffusePath", "");
    if (!diffPath.empty() && diffPath != mat.EffectiveDiffuseMapPath())
    {
        TextureHandle oldD, oldS;
        mat.ResolveTextures(oldD, oldS);
        if (oldD.IsValid())
            resources.DestroyTexture(oldD);
        if (mat.m_Material)
        {
            mat.m_Material->DiffuseMap = TextureHandle{};
            mat.m_Material->DiffuseMapPath.clear();
        }
        mat.DiffuseMap     = resources.CreateTextureFromFile(diffPath);
        mat.DiffuseMapPath = mat.DiffuseMap.IsValid() ? diffPath : "";
    }

    std::string specPath = mj.value("specularPath", "");
    if (!specPath.empty() && specPath != mat.EffectiveSpecularMapPath())
    {
        TextureHandle oldD, oldS;
        mat.ResolveTextures(oldD, oldS);
        if (oldS.IsValid())
            resources.DestroyTexture(oldS);
        if (mat.m_Material)
        {
            mat.m_Material->SpecularMap = TextureHandle{};
            mat.m_Material->SpecularMapPath.clear();
        }
        mat.SpecularMap     = resources.CreateTextureFromFile(specPath);
        mat.SpecularMapPath = mat.SpecularMap.IsValid() ? specPath : "";
    }
}

json SerializeRigidBody(const RigidBody3DComponent& rigidBody)
{
    json shape;
    shape["type"] = static_cast<int>(rigidBody.ShapeDesc.Type);
    shape["halfExtents"] = {
        rigidBody.ShapeDesc.HalfExtents.x,
        rigidBody.ShapeDesc.HalfExtents.y,
        rigidBody.ShapeDesc.HalfExtents.z
    };
    shape["radius"] = rigidBody.ShapeDesc.Radius;
    shape["halfHeight"] = rigidBody.ShapeDesc.HalfHeight;

    json rigidBodyJson;
    rigidBodyJson["syncMode"] = static_cast<int>(rigidBody.SyncMode);
    rigidBodyJson["bodyType"] = static_cast<int>(rigidBody.BodyDesc.Type);
    rigidBodyJson["mass"] = rigidBody.BodyDesc.Mass;
    rigidBodyJson["linearDamping"] = rigidBody.BodyDesc.LinearDamping;
    rigidBodyJson["angularDamping"] = rigidBody.BodyDesc.AngularDamping;
    rigidBodyJson["enableGravity"] = rigidBody.BodyDesc.EnableGravity;
    rigidBodyJson["staticFriction"] = rigidBody.BodyDesc.StaticFriction;
    rigidBodyJson["dynamicFriction"] = rigidBody.BodyDesc.DynamicFriction;
    rigidBodyJson["restitution"] = rigidBody.BodyDesc.Restitution;
    rigidBodyJson["shape"] = std::move(shape);
    return rigidBodyJson;
}

RigidBody3DComponent DeserializeRigidBody(const json& rbj)
{
    RigidBody3DComponent rigidBody{};
    rigidBody.SyncMode = static_cast<RigidBodySyncMode>(rbj.value("syncMode", static_cast<int>(RigidBodySyncMode::Auto)));
    rigidBody.BodyDesc.Type = static_cast<PhysicsBodyType>(rbj.value("bodyType", static_cast<int>(PhysicsBodyType::Dynamic)));
    rigidBody.BodyDesc.Mass = rbj.value("mass", 1.0f);
    rigidBody.BodyDesc.LinearDamping = rbj.value("linearDamping", 0.0f);
    rigidBody.BodyDesc.AngularDamping = rbj.value("angularDamping", 0.05f);
    rigidBody.BodyDesc.EnableGravity = rbj.value("enableGravity", true);
    rigidBody.BodyDesc.StaticFriction = rbj.value("staticFriction", 0.5f);
    rigidBody.BodyDesc.DynamicFriction = rbj.value("dynamicFriction", 0.5f);
    rigidBody.BodyDesc.Restitution = rbj.value("restitution", 0.1f);

    if (rbj.contains("shape") && rbj["shape"].is_object()) {
        const auto& shapeJson = rbj["shape"];
        rigidBody.ShapeDesc.Type = static_cast<PhysicsColliderShapeType>(
            shapeJson.value("type", static_cast<int>(PhysicsColliderShapeType::Box)));
        if (shapeJson.contains("halfExtents") && shapeJson["halfExtents"].is_array() && shapeJson["halfExtents"].size() >= 3) {
            rigidBody.ShapeDesc.HalfExtents.x = shapeJson["halfExtents"][0].get<float>();
            rigidBody.ShapeDesc.HalfExtents.y = shapeJson["halfExtents"][1].get<float>();
            rigidBody.ShapeDesc.HalfExtents.z = shapeJson["halfExtents"][2].get<float>();
        }
        rigidBody.ShapeDesc.Radius = shapeJson.value("radius", 0.5f);
        rigidBody.ShapeDesc.HalfHeight = shapeJson.value("halfHeight", 0.5f);
    }

    return rigidBody;
}

} // namespace

SceneSerializer::SceneSerializer(Scene* scene, World* world) : m_Scene(scene), m_World(world) {}

bool SceneSerializer::SaveToFile(const std::string& path) {
    json j;
    j["version"] = kSceneFormatVersion;
    j["objects"]  = json::array();

    m_Scene->ForEach<TagComponent>([&](EntityHandle handle, const UUID&, TagComponent& tag) {
        auto* idComp = m_Scene->TryGetComponent<IDComponent>(handle);
        auto* transformComp = m_Scene->TryGetComponent<TransformComponent>(handle);
        auto* meshPtr = m_Scene->TryGetComponent<MeshComponent>(handle);
        auto* colorPtr = m_Scene->TryGetComponent<ColorComponent>(handle);
        auto* visibilityPtr = m_Scene->TryGetComponent<VisibilityComponent>(handle);
        if (!idComp || !transformComp || !meshPtr || !colorPtr || !visibilityPtr)
            return;
        auto& mesh = *meshPtr;
        auto& color = *colorPtr;
        auto& visibility = *visibilityPtr;
        auto& transform = transformComp->ObjectTransform;
        json o;
        o["name"]    = tag.Name;
        o["uuid"]    = boost::uuids::to_string(idComp->Value);
        o["visible"] = visibility.Visible;
        o["color"]   = { color.Color.r, color.Color.g, color.Color.b, color.Color.a };

        o["position"] = { transform.Position.x, transform.Position.y, transform.Position.z };
        o["rotation"] = { transform.Rotation.x, transform.Rotation.y, transform.Rotation.z };
        o["scale"]    = { transform.Scale.x,    transform.Scale.y,    transform.Scale.z    };

        // Retrieve SourcePath via Scene's accessor (avoids exposing entt publicly)
        o["meshPath"] = mesh.SourcePath;

        json materials = json::array();
        for (auto& meshItem : mesh.Meshes)
        {
            if (!meshItem.Mat)
                meshItem.Mat = MaterialInstance::FromBase(
                    std::make_shared<Material>(RenderFacade::GetDefaultMeshShader()));
            json matObj;
            matObj["diffusePath"]   = meshItem.Mat->EffectiveDiffuseMapPath();
            matObj["specularPath"]  = meshItem.Mat->EffectiveSpecularMapPath();
            matObj["shininess"]     = meshItem.Mat->Shininess;
            matObj["specularScale"] = meshItem.Mat->SpecularScale;
            materials.push_back(matObj);
        }
        o["materials"] = materials;

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

        if (const auto* rigidBody = m_Scene->TryGetComponent<RigidBody3DComponent>(handle))
            o["rigidBody"] = SerializeRigidBody(*rigidBody);

        j["objects"].push_back(o);
    });

    if (!FileSystem::WriteFileText(path, j.dump(4))) {
        CHE_CORE_ERROR("SceneSerializer: cannot write to {}", path);
        return false;
    }
    CHE_CORE_INFO("Scene saved: {}", path);
    return true;
}

bool SceneSerializer::LoadFromFile(const std::string& path, RenderResourceManager& resources) {
    if (!FileSystem::Exists(path)) {
        CHE_CORE_ERROR("SceneSerializer: cannot read {}", path);
        return false;
    }

    json j;
    try {
        j = json::parse(FileSystem::ReadFileText(path));
    }
    catch (const std::exception& e) {
        CHE_CORE_ERROR("SceneSerializer: JSON parse error: {}", e.what());
        return false;
    }

    // Validate top-level structure
    if (!j.is_object() || !j.contains("objects") || !j["objects"].is_array()) {
        CHE_CORE_ERROR("SceneSerializer: malformed scene file (missing 'objects' array)");
        return false;
    }
    if (!j.contains("version") || !j["version"].is_number_integer()) {
        CHE_CORE_ERROR("SceneSerializer: malformed scene file (missing integer 'version')");
        return false;
    }
    const int version = j["version"].get<int>();
    if (version != 2 && version != kSceneFormatVersion) {
        CHE_CORE_ERROR("SceneSerializer: unsupported scene format version {} (expected 2 or {})",
            version, kSceneFormatVersion);
        return false;
    }

    // Clear existing scene GPU resources first
    m_Scene->ForEach<MeshComponent>([&](EntityHandle, const UUID&, MeshComponent& meshComp) {
        for (auto& mesh : meshComp.Meshes) {
            if (mesh.GetVertexArray().IsValid())
                resources.DestroyVertexArray(mesh.GetVertexArray());
        }
        DestroyUniqueMeshTextures(meshComp, resources);
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
        if (!o.contains("uuid") || !o["uuid"].is_string()) {
            CHE_CORE_WARN("SceneSerializer: skipping object '{}' without valid string 'uuid'", name);
            continue;
        }

        UUID entityUUID = boost::uuids::nil_uuid();
        if (!TryParseUUIDString(o["uuid"].get<std::string>(), entityUUID)) {
            CHE_CORE_WARN("SceneSerializer: skipping object '{}' with invalid uuid", name);
            continue;
        }

        EntityHandle handle{};

        if (!meshPath.empty()) {
            // Re-import model from disk
            auto result = ModelLoader::Load(meshPath, RenderFacade::GetDefaultMeshShader());
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

                handle = m_Scene->CreateEntity(name, entityUUID);
                if (auto* meshComp = m_Scene->TryGetComponent<MeshComponent>(handle)) {
                    meshComp->Meshes = std::move(result.meshes);
                    meshComp->SourcePath = meshPath;
                }
            }
        }

        if (!m_Scene->IsEntityHandleValid(handle))
            handle = m_Scene->CreateEntity(name, entityUUID);
        auto* obj = m_Scene->TryGetEntity(handle);
        if (!obj) continue;

        auto& transform = obj->GetComponent<TransformComponent>().ObjectTransform;
        auto& color = obj->GetComponent<ColorComponent>().Color;
        auto& visible = obj->GetComponent<VisibilityComponent>().Visible;

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

        // Десериализация материалов: materials[] по сабмешам; v2 material — один на все сабмеши (шаринг ptr)
        if (auto* meshCompForMat = m_Scene->TryGetComponent<MeshComponent>(handle))
        {
            for (auto& meshItem : meshCompForMat->Meshes)
            {
                if (!meshItem.Mat)
                    meshItem.Mat = MaterialInstance::FromBase(
                        std::make_shared<Material>(RenderFacade::GetDefaultMeshShader()));
            }

            if (o.contains("materials") && o["materials"].is_array())
            {
                const auto& arr = o["materials"];
                for (size_t mi = 0; mi < arr.size() && mi < meshCompForMat->Meshes.size(); ++mi)
                {
                    if (arr[mi].is_object())
                        ApplyMaterialFromJson(arr[mi], *meshCompForMat->Meshes[mi].Mat, resources);
                }
            }
            else if (o.contains("material") && o["material"].is_object())
            {
                auto& meshes = meshCompForMat->Meshes;
                if (!meshes.empty())
                {
                    ApplyMaterialFromJson(o["material"], *meshes[0].Mat, resources);
                    for (size_t i = 1; i < meshes.size(); ++i)
                        meshes[i].Mat = meshes[0].Mat;
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

        if (version >= 3 && o.contains("rigidBody") && o["rigidBody"].is_object()) {
            const RigidBody3DComponent rigidBody = DeserializeRigidBody(o["rigidBody"]);
            if (obj->HasComponent<RigidBody3DComponent>()) {
                if (m_World)
                    m_World->DestroyRigidBodyRuntime(handle);
                obj->RemoveComponent<RigidBody3DComponent>();
            }
            obj->AddComponent<RigidBody3DComponent>(RigidBody3DComponent{ rigidBody });
        } else if (obj->HasComponent<RigidBody3DComponent>()) {
            if (m_World)
                m_World->DestroyRigidBodyRuntime(handle);
            obj->RemoveComponent<RigidBody3DComponent>();
        }
    }

    if (m_World)
        m_World->RebuildPhysicsRuntime();

    CHE_CORE_INFO("Scene loaded: {}", path);
    return true;
}

// ── In-memory snapshot ────────────────────────────────────────────────────────

nlohmann::json SceneSerializer::SerializeToJson()
{
    json j;
    j["version"] = kSceneFormatVersion;
    j["objects"]  = json::array();

    m_Scene->ForEach<TagComponent>([&](EntityHandle handle, const UUID&, TagComponent& tag) {
        auto* idComp        = m_Scene->TryGetComponent<IDComponent>(handle);
        auto* transformComp = m_Scene->TryGetComponent<TransformComponent>(handle);
        auto* meshPtr       = m_Scene->TryGetComponent<MeshComponent>(handle);
        auto* colorPtr      = m_Scene->TryGetComponent<ColorComponent>(handle);
        auto* visibilityPtr = m_Scene->TryGetComponent<VisibilityComponent>(handle);
        if (!idComp || !transformComp || !meshPtr || !colorPtr || !visibilityPtr)
            return;

        auto& mesh       = *meshPtr;
        auto& color      = *colorPtr;
        auto& visibility = *visibilityPtr;
        auto& transform  = transformComp->ObjectTransform;

        json o;
        o["name"]    = tag.Name;
        o["uuid"]    = boost::uuids::to_string(idComp->Value);
        o["visible"] = visibility.Visible;
        o["color"]   = { color.Color.r, color.Color.g, color.Color.b, color.Color.a };
        o["position"] = { transform.Position.x, transform.Position.y, transform.Position.z };
        o["rotation"] = { transform.Rotation.x, transform.Rotation.y, transform.Rotation.z };
        o["scale"]    = { transform.Scale.x,    transform.Scale.y,    transform.Scale.z    };
        o["meshPath"] = mesh.SourcePath;

        json materials = json::array();
        for (auto& meshItem : mesh.Meshes)
        {
            if (!meshItem.Mat)
                meshItem.Mat = MaterialInstance::FromBase(
                    std::make_shared<Material>(RenderFacade::GetDefaultMeshShader()));
            json matObj;
            matObj["diffusePath"]   = meshItem.Mat->EffectiveDiffuseMapPath();
            matObj["specularPath"]  = meshItem.Mat->EffectiveSpecularMapPath();
            matObj["shininess"]     = meshItem.Mat->Shininess;
            matObj["specularScale"] = meshItem.Mat->SpecularScale;
            materials.push_back(matObj);
        }
        o["materials"] = materials;

        if (const auto* lightComp = m_Scene->TryGetComponent<LightComponent>(handle)) {
            const auto& ld = lightComp->LightData;
            json light;
            light["type"]      = static_cast<int>(ld.Type);
            light["color"]     = { ld.Color.r, ld.Color.g, ld.Color.b };
            light["intensity"] = ld.Intensity;
            light["range"]     = ld.Range;
            light["innerCone"] = ld.InnerCone;
            light["outerCone"] = ld.OuterCone;
            o["light"] = light;
        }

        if (const auto* rigidBody = m_Scene->TryGetComponent<RigidBody3DComponent>(handle))
            o["rigidBody"] = SerializeRigidBody(*rigidBody);

        j["objects"].push_back(o);
    });

    return j;
}

bool SceneSerializer::DeserializeFromJson(const nlohmann::json& data, RenderResourceManager& resources)
{
    if (!data.is_object() || !data.contains("objects") || !data["objects"].is_array())
        return false;

    const int version = data.value("version", kSceneFormatVersion);
    if (version != 2 && version != kSceneFormatVersion)
        return false;

    // Очистить GPU-ресурсы текущей сцены
    m_Scene->ForEach<MeshComponent>([&](EntityHandle, const UUID&, MeshComponent& meshComp) {
        for (auto& mesh : meshComp.Meshes) {
            if (mesh.GetVertexArray().IsValid())
                resources.DestroyVertexArray(mesh.GetVertexArray());
        }
        DestroyUniqueMeshTextures(meshComp, resources);
    });
    m_Scene->Clear();

    auto readFloats = [](const json& arr, size_t count, float* out) -> bool {
        if (!arr.is_array() || arr.size() < count) return false;
        for (size_t i = 0; i < count; ++i) {
            if (!arr[i].is_number()) return false;
            out[i] = arr[i].get<float>();
        }
        return true;
    };

    for (auto& o : data["objects"]) {
        if (!o.is_object()) continue;

        std::string name     = o.value("name", "Object");
        std::string meshPath = o.value("meshPath", "");

        if (!o.contains("uuid") || !o["uuid"].is_string()) continue;

        UUID entityUUID = boost::uuids::nil_uuid();
        if (!TryParseUUIDString(o["uuid"].get<std::string>(), entityUUID)) continue;

        EntityHandle handle{};

        if (!meshPath.empty()) {
            auto result = ModelLoader::Load(meshPath, RenderFacade::GetDefaultMeshShader());
            if (result.success) {
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

                handle = m_Scene->CreateEntity(name, entityUUID);
                if (auto* meshComp = m_Scene->TryGetComponent<MeshComponent>(handle)) {
                    meshComp->Meshes    = std::move(result.meshes);
                    meshComp->SourcePath = meshPath;
                }
            }
        }

        if (!m_Scene->IsEntityHandleValid(handle))
            handle = m_Scene->CreateEntity(name, entityUUID);
        auto* obj = m_Scene->TryGetEntity(handle);
        if (!obj) continue;

        auto& transform = obj->GetComponent<TransformComponent>().ObjectTransform;
        auto& color     = obj->GetComponent<ColorComponent>().Color;
        auto& visible   = obj->GetComponent<VisibilityComponent>().Visible;

        float v3[3], v4[4];
        if (o.contains("position") && readFloats(o["position"], 3, v3))
            transform.Position = { v3[0], v3[1], v3[2] };
        if (o.contains("rotation") && readFloats(o["rotation"], 3, v3))
            transform.Rotation = { v3[0], v3[1], v3[2] };
        if (o.contains("scale") && readFloats(o["scale"], 3, v3))
            transform.Scale = { v3[0], v3[1], v3[2] };
        if (o.contains("color") && readFloats(o["color"], 4, v4))
            color = { v4[0], v4[1], v4[2], v4[3] };

        visible = o.value("visible", true);

        if (auto* meshCompForMat = m_Scene->TryGetComponent<MeshComponent>(handle)) {
            for (auto& meshItem : meshCompForMat->Meshes) {
                if (!meshItem.Mat)
                    meshItem.Mat = MaterialInstance::FromBase(
                        std::make_shared<Material>(RenderFacade::GetDefaultMeshShader()));
            }
            if (o.contains("materials") && o["materials"].is_array()) {
                const auto& arr = o["materials"];
                for (size_t mi = 0; mi < arr.size() && mi < meshCompForMat->Meshes.size(); ++mi)
                    if (arr[mi].is_object())
                        ApplyMaterialFromJson(arr[mi], *meshCompForMat->Meshes[mi].Mat, resources);
            }
        }

        if (o.contains("light") && o["light"].is_object()) {
            const auto& lj = o["light"];
            int typeVal = lj.value("type", -1);
            if (typeVal < 0 || typeVal > 2) typeVal = -1;
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

        if (version >= 3 && o.contains("rigidBody") && o["rigidBody"].is_object()) {
            const RigidBody3DComponent rigidBody = DeserializeRigidBody(o["rigidBody"]);
            if (obj->HasComponent<RigidBody3DComponent>()) {
                if (m_World) m_World->DestroyRigidBodyRuntime(handle);
                obj->RemoveComponent<RigidBody3DComponent>();
            }
            obj->AddComponent<RigidBody3DComponent>(RigidBody3DComponent{ rigidBody });
        } else if (obj->HasComponent<RigidBody3DComponent>()) {
            if (m_World) m_World->DestroyRigidBodyRuntime(handle);
            obj->RemoveComponent<RigidBody3DComponent>();
        }
    }

    if (m_World)
        m_World->RebuildPhysicsRuntime();

    return true;
}

} // namespace CHEngine
