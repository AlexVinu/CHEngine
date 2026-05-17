#include "chepch.h"
#include "SceneSerializer.h"

#include "FileSystem/FileSystem.h"

#include <nlohmann/json.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cstddef>
#include <memory>
#include <unordered_set>
#include <CHEngine/Mesh/Material.h>
#include <CHEngine/Mesh/PrimitiveMeshFactory.h>
#include <CHEngine/ResourceManager/ResourceManager.h>
#include <CHEngine/Application.h>

#include <CHEngine/Scene/Components.h>
#include "Entity.h"
#include <Log/Log.h>
#include <glm/glm.hpp>
#include <filesystem>

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

static void DestroyTexturesForInstance(MaterialInstance* p,
                                       std::unordered_set<Material*>& baseDiffuseDestroyed,
                                       std::unordered_set<Material*>& baseSpecDestroyed)
{
    TextureHandle d, s;
    p->ResolveTextures(d, s);

    if (d.IsValid())
    {
        if (p->DiffuseMap.IsValid())
            CHEngine::Application::Get().Render().DestroyTexture(d);
        else if (p->m_Material && p->m_Material->DiffuseMap.IsValid())
        {
            if (baseDiffuseDestroyed.insert(p->m_Material.get()).second)
                CHEngine::Application::Get().Render().DestroyTexture(d);
        }
    }
    if (s.IsValid())
    {
        if (p->SpecularMap.IsValid())
            CHEngine::Application::Get().Render().DestroyTexture(s);
        else if (p->m_Material && p->m_Material->SpecularMap.IsValid())
        {
            if (baseSpecDestroyed.insert(p->m_Material.get()).second)
                CHEngine::Application::Get().Render().DestroyTexture(s);
        }
    }
}

void DestroyUniqueMeshTextures(MeshComponent& meshComp)
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
        DestroyTexturesForInstance(p, baseDiffuseDestroyed, baseSpecDestroyed);
    }
}

void ApplyMaterialFromJson(const json& mj, MaterialInstance& mat)
{
    mat.Shininess     = mj.value("shininess", 32.0f);
    mat.SpecularScale = mj.value("specularScale", 1.0f);
    mat.Roughness     = mj.value("roughness", 0.5f);
    mat.Metallic      = mj.value("metallic", 0.0f);
    mat.AO            = mj.value("ao", 1.0f);

    if (mj.value("usePBR", false))
        mat.GetMaterial()->MaterialFlags |= kPBR_EnablePBR;
    else
        mat.GetMaterial()->MaterialFlags &= ~kPBR_EnablePBR;

    std::string diffPath = mj.value("diffusePath", "");
    if (!diffPath.empty() && diffPath != mat.EffectiveDiffuseMapPath())
    {
        TextureHandle oldD, oldS;
        mat.ResolveTextures(oldD, oldS);
        if (oldD.IsValid())
            Application::Get().Resources().Unload(oldD);
        if (mat.m_Material)
        {
            mat.m_Material->DiffuseMap = TextureHandle{};
            mat.m_Material->DiffuseMapPath.clear();
        }
        mat.DiffuseMap     = Application::Get().Resources().Load<TextureHandle>(std::filesystem::path(diffPath));
        mat.DiffuseMapPath = mat.DiffuseMap.IsValid() ? diffPath : "";
    }

    std::string specPath = mj.value("specularPath", "");
    if (!specPath.empty() && specPath != mat.EffectiveSpecularMapPath())
    {
        TextureHandle oldD, oldS;
        mat.ResolveTextures(oldD, oldS);
        if (oldS.IsValid())
            Application::Get().Resources().Unload(oldS);
        if (mat.m_Material)
        {
            mat.m_Material->SpecularMap = TextureHandle{};
            mat.m_Material->SpecularMapPath.clear();
        }
        mat.SpecularMap     = Application::Get().Resources().Load<TextureHandle>(std::filesystem::path(specPath));
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

bool DeserializeSceneData(Ref<Scene> scene, const json& data)
{
    if (!data.is_object() || !data.contains("objects") || !data["objects"].is_array()) {
        CHE_CORE_ERROR("SceneSerializer: malformed scene data (missing 'objects' array)");
        return false;
    }
    if (!data.contains("version") || !data["version"].is_number_integer()) {
        CHE_CORE_ERROR("SceneSerializer: malformed scene data (missing integer 'version')");
        return false;
    }

    const int version = data["version"].get<int>();
    if (version != 2 && version != kSceneFormatVersion) {
        CHE_CORE_ERROR("SceneSerializer: unsupported scene format version {} (expected 2 or {})",
            version, kSceneFormatVersion);
        return false;
    }

    scene->ForEach<MeshComponent>([&](EntityHandle, const UUID&, MeshComponent& meshComp) {
        // Mesh buffers are released by Mesh's destructor when MeshComponent is cleared.
        // TODO (Phase 6): explicit factory->Delete(BufferHandle) once Mesh tracks ownership.
        DestroyUniqueMeshTextures(meshComp);
    });
    scene->Clear();

    auto readFloats = [](const json& arr, size_t count, float* out) -> bool {
        if (!arr.is_array() || arr.size() < count) return false;
        for (size_t i = 0; i < count; ++i) {
            if (!arr[i].is_number()) return false;
            out[i] = arr[i].get<float>();
        }
        return true;
    };

    for (auto& o : data["objects"]) {
        if (!o.is_object()) {
            CHE_CORE_WARN("SceneSerializer: skipping non-object entry in 'objects' array");
            continue;
        }

        std::string name = o.value("name", "Object");
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
        std::vector<Mesh> importedMeshes;
        bool hasImportedMeshes = false;

        if (!meshPath.empty()) {
            if (meshPath == ":primitive:cube")
            {
                Mesh cubeMesh = PrimitiveMeshFactory::CreateCube(1.0f, { 0.8f, 0.8f, 0.8f });
                cubeMesh.Mat = MaterialInstance::FromBase(
                    std::make_shared<Material>(CHEngine::Application::Get().Render().GetDefaultMeshShader()));
                importedMeshes.push_back(std::move(cubeMesh));
                hasImportedMeshes = true;
            }
            else if (meshPath == ":primitive:sphere")
            {
                ShaderHandle sphereShader = CHEngine::Application::Get().Render().GetDefaultSphereImpostorShader();
                if (!sphereShader.IsValid())
                    sphereShader = CHEngine::Application::Get().Render().GetDefaultMeshShader();
                Mesh sphereMesh = PrimitiveMeshFactory::CreateSphereImpostor({ 0.6f, 0.7f, 0.9f });
                sphereMesh.Mat = MaterialInstance::FromBase(
                    std::make_shared<Material>(sphereShader));
                importedMeshes.push_back(std::move(sphereMesh));
                hasImportedMeshes = true;
            }
            else
            {
                auto modelHandle = Application::Get().Resources().Load<ModelHandle>(
                    std::filesystem::path(meshPath), CHEngine::Application::Get().Render().GetDefaultMeshShader());
                const LoadedModel* result = modelHandle.IsValid()
                    ? Application::Get().Resources().GetModel(modelHandle) : nullptr;
                if (result && !result->meshes.empty()) {
                    importedMeshes = result->meshes; // copy — Mesh copy-ctor AddRefs GPU records

                    glm::vec3 centroid(0.0f);
                    size_t totalVerts = 0;
                    for (auto& mesh : importedMeshes) {
                        for (const auto& v : mesh.GetVertices()) {
                            centroid += v.Position;
                            ++totalVerts;
                        }
                    }
                    if (totalVerts > 0) centroid /= static_cast<float>(totalVerts);

                    if (totalVerts > 0 && glm::length(centroid) > 1e-5f) {
                        for (auto& mesh : importedMeshes) {
                            auto verts = mesh.GetVertices();
                            for (auto& v : verts) v.Position -= centroid;
                            mesh.Build(verts, mesh.GetIndices());
                        }
                    }

                    hasImportedMeshes = true;
                } else {
                    std::string normalizedPath = meshPath;
                    std::filesystem::path fsPath(meshPath);
                    if (!fsPath.empty())
                        normalizedPath = fsPath.lexically_normal().string();

                    CHE_CORE_WARN(
                        "SceneSerializer: model load failed for entity='{}' uuid={} meshPath='{}' normalized='{}'",
                        name,
                        boost::uuids::to_string(entityUUID),
                        meshPath,
                        normalizedPath);
                }
            }
        }

        if (!scene->IsEntityHandleValid(handle))
            handle = scene->CreateEntity(name, entityUUID);
        auto* obj = scene->TryGetEntity(handle);
        if (!obj) continue;

        if (hasImportedMeshes && obj->HasComponent<MeshComponent>())
        {
            auto* meshComp = &obj->GetComponent<MeshComponent>();
            meshComp->Meshes = std::move(importedMeshes);
            meshComp->SourcePath = meshPath;
        } else if (!meshPath.empty() && obj->HasComponent<MeshComponent>()) {
            auto* meshComp = &obj->GetComponent<MeshComponent>();
            meshComp->SourcePath = meshPath;
        }

        auto& transform = obj->GetComponent<TransformComponent>().ObjectTransform;
        auto& color = obj->GetComponent<ColorComponent>().Color;
        auto& visible = obj->GetComponent<VisibilityComponent>().Visible;

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

        if (o.contains("camera") && o["camera"].is_object()) {
            const auto& cameraJson = o["camera"];
            if (!obj->HasComponent<CameraComponent>())
                obj->AddComponent<CameraComponent>(CameraComponent{});

            auto* cameraComp = &obj->GetComponent<CameraComponent>();
            cameraComp->Primary = cameraJson.value("primary", cameraComp->Primary);
            cameraComp->FixedAspectRatio = cameraJson.value("fixedAspectRatio", cameraComp->FixedAspectRatio);

            const float defaultAspectRatio = 16.0f / 9.0f;
            const float serializedAspectRatio = cameraJson.value("aspectRatio", defaultAspectRatio);
            const float safeAspectRatio = serializedAspectRatio > 0.001f ? serializedAspectRatio : defaultAspectRatio;
            cameraComp->Camera.SetViewportSize(static_cast<uint32_t>(safeAspectRatio * 1000.0f), 1000u);

            if (cameraJson.contains("projectionType") && cameraJson["projectionType"].is_number_integer()) {
                const int projectionType = cameraJson["projectionType"].get<int>();
                cameraComp->Camera.SetProjectionType(static_cast<SceneCamera::ProjectionType>(projectionType));

                if (cameraJson.contains("perspective") && cameraJson["perspective"].is_object()) {
                    const auto& perspective = cameraJson["perspective"];
                    cameraComp->Camera.SetPerspectiveVerticalFOV(
                        perspective.value("verticalFOV", cameraComp->Camera.GetPerspectiveVerticalFOV()));
                    cameraComp->Camera.SetPerspectiveNearClip(
                        perspective.value("nearClip", cameraComp->Camera.GetPerspectiveNearClip()));
                    cameraComp->Camera.SetPerspectiveFarClip(
                        perspective.value("farClip", cameraComp->Camera.GetPerspectiveFarClip()));
                }

                if (cameraJson.contains("orthographic") && cameraJson["orthographic"].is_object()) {
                    const auto& orthographic = cameraJson["orthographic"];
                    cameraComp->Camera.SetOrthographicSize(
                        orthographic.value("size", cameraComp->Camera.GetOrthographicSize()));
                    cameraComp->Camera.SetOrthographicNearClip(
                        orthographic.value("nearClip", cameraComp->Camera.GetOrthographicNearClip()));
                    cameraComp->Camera.SetOrthographicFarClip(
                        orthographic.value("farClip", cameraComp->Camera.GetOrthographicFarClip()));
                }
            } else {
                // Legacy fallback (v2/v3): migrate old scalar camera fields into SceneCamera.
                const float legacyFovDegrees = cameraJson.value("fov", 45.0f);
                const float legacyNearClip = cameraJson.value("nearClip", 0.1f);
                const float legacyFarClip = cameraJson.value("farClip", 1000.0f);
                cameraComp->Camera.SetPerspective(
                    glm::radians(legacyFovDegrees),
                    legacyNearClip,
                    legacyFarClip);
            }
        }

        if (obj->HasComponent<MeshComponent>())
        {
            auto* meshCompForMat = &obj->GetComponent<MeshComponent>();
            for (auto& meshItem : meshCompForMat->Meshes)
            {
                if (!meshItem.Mat)
                    meshItem.Mat = MaterialInstance::FromBase(
                        std::make_shared<Material>(CHEngine::Application::Get().Render().GetDefaultMeshShader()));
            }

            if (o.contains("materials") && o["materials"].is_array())
            {
                const auto& arr = o["materials"];
                for (size_t mi = 0; mi < arr.size() && mi < meshCompForMat->Meshes.size(); ++mi)
                {
                    if (arr[mi].is_object())
                        ApplyMaterialFromJson(arr[mi], *meshCompForMat->Meshes[mi].Mat);
                }
            }
            else if (o.contains("material") && o["material"].is_object())
            {
                auto& meshes = meshCompForMat->Meshes;
                if (!meshes.empty())
                {
                    ApplyMaterialFromJson(o["material"], *meshes[0].Mat);
                    for (size_t i = 1; i < meshes.size(); ++i)
                        meshes[i].Mat = meshes[0].Mat;
                }
            }
        }

        if (o.contains("light") && o["light"].is_object()) {
            const auto& lj = o["light"];
            int typeVal = lj.value("type", -1);
            if (typeVal >= 0 && typeVal <= 2) {
                Light lightData;
                lightData.Type = static_cast<LightType>(typeVal);
                float lc[3];
                if (lj.contains("color") && readFloats(lj["color"], 3, lc))
                    lightData.Color = { lc[0], lc[1], lc[2] };
                lightData.Intensity = lj.value("intensity", 1.0f);
                lightData.Range     = lj.value("range",     10.0f);
                lightData.InnerCone = lj.value("innerCone", 12.5f);
                lightData.OuterCone = lj.value("outerCone", 17.5f);
                if (obj->HasComponent<LightComponent>())
                    obj->RemoveComponent<LightComponent>();
                obj->AddComponent<LightComponent>(LightComponent{ lightData });
            } else {
                CHE_CORE_WARN("SceneSerializer: invalid light type {} for '{}', skipping", typeVal, name);
            }
        } else if (obj->HasComponent<LightComponent>()) {
            obj->RemoveComponent<LightComponent>();
        }

        if (version >= 3 && o.contains("rigidBody") && o["rigidBody"].is_object()) {
            const RigidBody3DComponent rigidBody = DeserializeRigidBody(o["rigidBody"]);
            if (obj->HasComponent<RigidBody3DComponent>())
                obj->RemoveComponent<RigidBody3DComponent>();
            obj->AddComponent<RigidBody3DComponent>(RigidBody3DComponent{ rigidBody });
        } else if (obj->HasComponent<RigidBody3DComponent>()) {
            obj->RemoveComponent<RigidBody3DComponent>();
        }

        // Script component (multi-script; backward compat with legacy "script")
        auto scriptEntries = DeserializeScripts(o);
        if (!scriptEntries.empty())
        {
            if (obj->HasComponent<ScriptComponent>())
                obj->RemoveComponent<ScriptComponent>();
            obj->AddComponent<ScriptComponent>(ScriptComponent{ std::move(scriptEntries) });
        }
    }

    // World-level scripts
    scene->WorldScripts.clear();
    if (data.contains("world_scripts") && data["world_scripts"].is_array())
    {
        for (const auto& e : data["world_scripts"])
        {
            if (!e.is_object()) continue;
            std::string path = e.value("path", "");
            if (path.empty()) continue;
            scene->WorldScripts.push_back(ScriptEntry{ std::move(path), e.value("enabled", true) });
        }
    }

    return true;
}

} // namespace

bool SceneSerializer::SaveToFile(Ref<Scene> scene, const std::string& path) {
    json j;
    j["version"] = kSceneFormatVersion;
    j["objects"]  = json::array();
    if (!scene->WorldScripts.empty())
        j["world_scripts"] = SerializeScripts(scene->WorldScripts);

    scene->ForEach<IDComponent>([&](EntityHandle handle, const UUID& uuid, IDComponent&) {
        Entity* entity = scene->TryGetEntity(handle);

        json o;
        o["uuid"] = boost::uuids::to_string(uuid);
        if (entity->HasComponent<TagComponent>())
        {
            auto component = entity->GetComponent<TagComponent>();
            o["name"] = component.Name;
        }
        if (entity->HasComponent<VisibilityComponent>())
        {
            auto component = entity->GetComponent<VisibilityComponent>();
            o["visible"] = component.Visible;
        }
        if (entity->HasComponent<ColorComponent>())
        {
            auto component = entity->GetComponent<ColorComponent>();
            o["color"] = { component.Color.r, component.Color.g, component.Color.b, component.Color.a };
        }
        if (entity->HasComponent<TransformComponent>())
        {
            auto& t = entity->GetComponent<TransformComponent>().ObjectTransform;
            o["position"] = { t.Position.x, t.Position.y, t.Position.z };
            o["rotation"] = { t.Rotation.x, t.Rotation.y, t.Rotation.z };
            o["scale"]    = { t.Scale.x,    t.Scale.y,    t.Scale.z    };
        }
        if (entity->HasComponent<MeshComponent>())
        {
            auto& meshComp = entity->GetComponent<MeshComponent>();
            o["meshPath"] = meshComp.SourcePath;

            json materials = json::array();
            for (const auto& meshItem : meshComp.Meshes)
            {
                // Do NOT mutate meshItem.Mat during serialization (would modify scene data).
                json matObj;
                if (meshItem.Mat) {
                    matObj["diffusePath"]   = meshItem.Mat->EffectiveDiffuseMapPath();
                    matObj["specularPath"]  = meshItem.Mat->EffectiveSpecularMapPath();
                    matObj["shininess"]     = meshItem.Mat->Shininess;
                    matObj["specularScale"] = meshItem.Mat->SpecularScale;
                    matObj["roughness"]     = meshItem.Mat->Roughness;
                    matObj["metallic"]      = meshItem.Mat->Metallic;
                    matObj["ao"]            = meshItem.Mat->AO;
                    matObj["usePBR"]        = meshItem.Mat->GetMaterial()
                                              ? (meshItem.Mat->GetMaterial()->MaterialFlags & kPBR_EnablePBR) != 0
                                              : false;
                } else {
                    matObj["diffusePath"] = ""; matObj["specularPath"] = "";
                    matObj["shininess"] = 32.0f; matObj["specularScale"] = 1.0f;
                    matObj["roughness"] = 0.5f;  matObj["metallic"] = 0.0f;
                    matObj["ao"] = 1.0f;          matObj["usePBR"] = false;
                }
                materials.push_back(matObj);
            }
            o["materials"] = materials;
        }
        if (entity->HasComponent<LightComponent>())
        {
            const auto& lightData = entity->GetComponent<LightComponent>().LightData;
            json light;
            light["type"]      = static_cast<int>(lightData.Type);
            light["color"]     = { lightData.Color.r, lightData.Color.g, lightData.Color.b };
            light["intensity"] = lightData.Intensity;
            light["range"]     = lightData.Range;
            light["innerCone"] = lightData.InnerCone;
            light["outerCone"] = lightData.OuterCone;
            o["light"] = light;
        }
        if (entity->HasComponent<ScriptComponent>())
        {
            const auto& sc = entity->GetComponent<ScriptComponent>();
            if (!sc.Scripts.empty())
                o["scripts"] = SerializeScripts(sc.Scripts);
        }
                if (entity->HasComponent<RigidBody3DComponent>())
            o["rigidBody"] = SerializeRigidBody(entity->GetComponent<RigidBody3DComponent>());

        if (entity->HasComponent<CameraComponent>())
        {
            const auto* cameraComp = &entity->GetComponent<CameraComponent>();
            json camera;
            camera["projectionType"] = static_cast<int>(cameraComp->Camera.GetProjectionType());
            camera["perspective"] = {
                { "verticalFOV", cameraComp->Camera.GetPerspectiveVerticalFOV() },
                { "nearClip",    cameraComp->Camera.GetPerspectiveNearClip() },
                { "farClip",     cameraComp->Camera.GetPerspectiveFarClip() }
            };
            camera["orthographic"] = {
                { "size",     cameraComp->Camera.GetOrthographicSize() },
                { "nearClip", cameraComp->Camera.GetOrthographicNearClip() },
                { "farClip",  cameraComp->Camera.GetOrthographicFarClip() }
            };
            camera["aspectRatio"]      = 16.0f / 9.0f;
            camera["fixedAspectRatio"] = cameraComp->FixedAspectRatio;
            camera["primary"]          = cameraComp->Primary;
            o["camera"] = camera;
        }

        j["objects"].push_back(o);
    });

    if (!FileSystem::WriteFileText(path, j.dump(4))) {
        CHE_CORE_ERROR("SceneSerializer: cannot write to {}", path);
        return false;
    }
    CHE_CORE_INFO("Scene saved: {}", path);
    return true;
}

Ref<Scene> SceneSerializer::LoadFromFile(const std::string& path) {
    if (!FileSystem::Exists(path)) {
        CHE_CORE_ERROR("SceneSerializer: cannot read {}", path);
        return nullptr;
    }

    json j;
    try {
        j = json::parse(FileSystem::ReadFileText(path));
    }
    catch (const std::exception& e) {
        CHE_CORE_ERROR("SceneSerializer: JSON parse error: {}", e.what());
        return nullptr;
    }
    Ref<Scene> loadedScene = MakeRef<Scene>();
    if (!DeserializeSceneData(loadedScene, j))
        return nullptr;

    CHE_CORE_INFO("Scene loaded: {}", path);
    return loadedScene;
}

// ── In-memory snapshot ────────────────────────────────────────────────────────

nlohmann::json SceneSerializer::SerializeToJson(Ref<Scene> scene)
{
    json j;
    j["version"] = kSceneFormatVersion;
    j["objects"]  = json::array();
    if (!scene->WorldScripts.empty())
        j["world_scripts"] = SerializeScripts(scene->WorldScripts);

    scene->ForEach<IDComponent>([&](EntityHandle handle, const UUID& uuid, IDComponent&) {
        Entity* entity = scene->TryGetEntity(handle);

        json o;
        o["uuid"] = boost::uuids::to_string(uuid);
        if (entity->HasComponent<TagComponent>())
        {
            auto component = entity->GetComponent<TagComponent>();
            o["name"] = component.Name;
        }
        if (entity->HasComponent<VisibilityComponent>())
        {
            auto component = entity->GetComponent<VisibilityComponent>();
            o["visible"] = component.Visible;
        }
        if (entity->HasComponent<ColorComponent>())
        {
            auto component = entity->GetComponent<ColorComponent>();
            o["color"] = { component.Color.r, component.Color.g, component.Color.b, component.Color.a };
        }
        if (entity->HasComponent<TransformComponent>())
        {
            auto& t = entity->GetComponent<TransformComponent>().ObjectTransform;
            o["position"] = { t.Position.x, t.Position.y, t.Position.z };
            o["rotation"] = { t.Rotation.x, t.Rotation.y, t.Rotation.z };
            o["scale"]    = { t.Scale.x,    t.Scale.y,    t.Scale.z    };
        }
        if (entity->HasComponent<MeshComponent>())
        {
            auto& meshComp = entity->GetComponent<MeshComponent>();
            o["meshPath"] = meshComp.SourcePath;

            json materials = json::array();
            for (const auto& meshItem : meshComp.Meshes)
            {
                // Do NOT mutate meshItem.Mat during serialization (would modify scene data).
                json matObj;
                if (meshItem.Mat) {
                    matObj["diffusePath"]   = meshItem.Mat->EffectiveDiffuseMapPath();
                    matObj["specularPath"]  = meshItem.Mat->EffectiveSpecularMapPath();
                    matObj["shininess"]     = meshItem.Mat->Shininess;
                    matObj["specularScale"] = meshItem.Mat->SpecularScale;
                    matObj["roughness"]     = meshItem.Mat->Roughness;
                    matObj["metallic"]      = meshItem.Mat->Metallic;
                    matObj["ao"]            = meshItem.Mat->AO;
                    matObj["usePBR"]        = meshItem.Mat->GetMaterial()
                                              ? (meshItem.Mat->GetMaterial()->MaterialFlags & kPBR_EnablePBR) != 0
                                              : false;
                } else {
                    matObj["diffusePath"] = ""; matObj["specularPath"] = "";
                    matObj["shininess"] = 32.0f; matObj["specularScale"] = 1.0f;
                    matObj["roughness"] = 0.5f;  matObj["metallic"] = 0.0f;
                    matObj["ao"] = 1.0f;          matObj["usePBR"] = false;
                }
                materials.push_back(matObj);
            }
            o["materials"] = materials;
        }
        if (entity->HasComponent<LightComponent>())
        {
            const auto& lightData = entity->GetComponent<LightComponent>().LightData;
            json light;
            light["type"]      = static_cast<int>(lightData.Type);
            light["color"]     = { lightData.Color.r, lightData.Color.g, lightData.Color.b };
            light["intensity"] = lightData.Intensity;
            light["range"]     = lightData.Range;
            light["innerCone"] = lightData.InnerCone;
            light["outerCone"] = lightData.OuterCone;
            o["light"] = light;
        }
        if (entity->HasComponent<ScriptComponent>())
        {
            const auto& sc = entity->GetComponent<ScriptComponent>();
            if (!sc.Scripts.empty())
                o["scripts"] = SerializeScripts(sc.Scripts);
        }
                if (entity->HasComponent<RigidBody3DComponent>())
            o["rigidBody"] = SerializeRigidBody(entity->GetComponent<RigidBody3DComponent>());

        if (entity->HasComponent<CameraComponent>())
        {
            const auto* cameraComp = &entity->GetComponent<CameraComponent>();
            json camera;
            camera["projectionType"] = static_cast<int>(cameraComp->Camera.GetProjectionType());
            camera["perspective"] = {
                { "verticalFOV", cameraComp->Camera.GetPerspectiveVerticalFOV() },
                { "nearClip",    cameraComp->Camera.GetPerspectiveNearClip() },
                { "farClip",     cameraComp->Camera.GetPerspectiveFarClip() }
            };
            camera["orthographic"] = {
                { "size",     cameraComp->Camera.GetOrthographicSize() },
                { "nearClip", cameraComp->Camera.GetOrthographicNearClip() },
                { "farClip",  cameraComp->Camera.GetOrthographicFarClip() }
            };
            camera["aspectRatio"]      = 16.0f / 9.0f;
            camera["fixedAspectRatio"] = cameraComp->FixedAspectRatio;
            camera["primary"]          = cameraComp->Primary;
            o["camera"] = camera;
        }

        j["objects"].push_back(o);
    });

    return j;
}

Ref<Scene> SceneSerializer::DeserializeFromJson(const nlohmann::json& data)
{
    Ref<Scene> loadedScene = MakeRef<Scene>();
    if (!DeserializeSceneData(loadedScene, data))
        return nullptr;
    return loadedScene;
}

} // namespace CHEngine
