#include "chepch.h"
#include "SceneSerializer.h"

#include "FileSystem/FileSystem.h"

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

namespace CHEngine {

namespace {
constexpr int kSceneFormatVersion = 3;

// ─── UI component helpers ─────────────────────────────────────────────────────

static json SerializeVec2(const glm::vec2& v)  { return { v.x, v.y }; }
static json SerializeVec4(const glm::vec4& v)  { return { v.r, v.g, v.b, v.a }; }

static glm::vec2 ReadVec2(const json& j, const char* key, glm::vec2 def = {})
{
    if (!j.contains(key) || !j[key].is_array() || j[key].size() < 2) return def;
    return { j[key][0].get<float>(), j[key][1].get<float>() };
}
static glm::vec4 ReadVec4(const json& j, const char* key, glm::vec4 def = {})
{
    if (!j.contains(key) || !j[key].is_array() || j[key].size() < 4) return def;
    return { j[key][0].get<float>(), j[key][1].get<float>(),
             j[key][2].get<float>(), j[key][3].get<float>() };
}

static json SerializeUIComponents(const Entity* entity)
{
    json ui = json::object();

    if (entity->HasComponent<UIOverlayCanvasComponent>())
    {
        const auto& c = entity->GetComponent<UIOverlayCanvasComponent>();
        ui["overlayCanvas"]["anchorMin"] = SerializeVec2(c.AnchorMin);
        ui["overlayCanvas"]["anchorMax"] = SerializeVec2(c.AnchorMax);
        ui["overlayCanvas"]["position"]  = SerializeVec2(c.Position);
        ui["overlayCanvas"]["size"]      = SerializeVec2(c.Size);
        ui["overlayCanvas"]["pivot"]     = SerializeVec2(c.Pivot);
        ui["overlayCanvas"]["sortOrder"] = c.SortOrder;
        ui["overlayCanvas"]["alpha"]     = c.Alpha;
    }
    if (entity->HasComponent<UIWorldCanvasComponent>())
    {
        const auto& c = entity->GetComponent<UIWorldCanvasComponent>();
        ui["worldCanvas"]["size"]        = SerializeVec2(c.Size);
        ui["worldCanvas"]["alpha"]       = c.Alpha;
        ui["worldCanvas"]["doubleSided"] = c.DoubleSided;
    }
    if (entity->HasComponent<UIRectTransformComponent>())
    {
        const auto& r = entity->GetComponent<UIRectTransformComponent>();
        ui["rectTransform"]["anchorMin"] = SerializeVec2(r.AnchorMin);
        ui["rectTransform"]["anchorMax"] = SerializeVec2(r.AnchorMax);
        ui["rectTransform"]["size"]      = r.Size;
        ui["rectTransform"]["pivot"]     = SerializeVec2(r.Pivot);
        ui["rectTransform"]["alpha"]     = r.Alpha;
        ui["rectTransform"]["zOrder"]    = r.ZOrder;
    }
    if (entity->HasComponent<ParentNodeComponent>())
    {
        const auto& p = entity->GetComponent<ParentNodeComponent>();
        ui["parentNode"]["value"] = static_cast<std::string>(p.Value);
    }
    if (entity->HasComponent<UIPanelComponent>())
    {
        const auto& p = entity->GetComponent<UIPanelComponent>();
        ui["panel"]["width"]        = p.Width;
        ui["panel"]["borderColor"]  = SerializeVec4(p.BorderColor);
        ui["panel"]["borderWidth"]  = p.BorderWidth;
        ui["panel"]["cornerRadius"] = p.CornerRadius;
    }
    if (entity->HasComponent<UITextComponent>())
    {
        const auto& t = entity->GetComponent<UITextComponent>();
        ui["text"]["text"]            = t.Text;
        ui["text"]["fontPath"]        = t.FontPath;
        ui["text"]["fontSize"]        = t.FontSize;
        ui["text"]["color"]           = SerializeVec4(t.Color);
        ui["text"]["hAlign"]          = static_cast<int>(t.HorizontalAlign);
        ui["text"]["vAlign"]          = static_cast<int>(t.VerticalAlign);
        ui["text"]["bold"]            = t.Bold;
        ui["text"]["italic"]          = t.Italic;
        ui["text"]["wordWrap"]        = t.WordWrap;
    }
    if (entity->HasComponent<UIButtonComponent>())
    {
        const auto& b = entity->GetComponent<UIButtonComponent>();
        ui["button"]["width"]         = b.Width;
        ui["button"]["normalColor"]   = SerializeVec4(b.NormalColor);
        ui["button"]["hoverColor"]    = SerializeVec4(b.HoverColor);
        ui["button"]["pressedColor"]  = SerializeVec4(b.PressedColor);
        ui["button"]["disabledColor"] = SerializeVec4(b.DisabledColor);
        ui["button"]["interactable"]  = b.Interactable;
        ui["button"]["cornerRadius"]  = b.CornerRadius;
    }
    if (entity->HasComponent<UISliderComponent>())
    {
        const auto& s = entity->GetComponent<UISliderComponent>();
        ui["slider"]["width"]           = s.Width;
        ui["slider"]["value"]           = s.Value;
        ui["slider"]["min"]             = s.Min;
        ui["slider"]["max"]             = s.Max;
        ui["slider"]["bgColor"]         = SerializeVec4(s.BackgroundColor);
        ui["slider"]["fillColor"]       = SerializeVec4(s.FillColor);
        ui["slider"]["handleColor"]     = SerializeVec4(s.HandleColor);
        ui["slider"]["handleSize"]      = s.HandleSize;
        ui["slider"]["interactable"]    = s.Interactable;
    }
    if (entity->HasComponent<UIImageComponent>())
    {
        const auto& i = entity->GetComponent<UIImageComponent>();
        ui["image"]["width"]          = i.Width;
        ui["image"]["texturePath"]    = i.TexturePath;
        ui["image"]["preserveAspect"] = i.PreserveAspect;
    }

    return ui;
}

static void DeserializeUIComponents(Entity* entity, const json& ui)
{
    // Migrate legacy UI color fields to ColorComponent (only if not already present).
    auto migrateColorToComponent = [&](const json& uj, const char* key, glm::vec4 def) {
        if (uj.contains(key) && !entity->HasComponent<ColorComponent>())
            entity->AddComponent<ColorComponent>(ColorComponent{ ReadVec4(uj, key, def) });
    };

    if (ui.contains("overlayCanvas") && ui["overlayCanvas"].is_object())
    {
        const auto& cj = ui["overlayCanvas"];
        UIOverlayCanvasComponent c;
        c.AnchorMin = ReadVec2(cj, "anchorMin", { 0.5f, 0.5f });
        c.AnchorMax = ReadVec2(cj, "anchorMax", { 0.5f, 0.5f });
        c.Position  = ReadVec2(cj, "position");
        c.Size      = ReadVec2(cj, "size", { 1.f, 1.f });
        c.Pivot     = ReadVec2(cj, "pivot", { 0.5f, 0.5f });
        c.SortOrder = cj.value("sortOrder", 0);
        c.Alpha     = cj.value("alpha", 1.f);
        if (entity->HasComponent<UIOverlayCanvasComponent>())
            entity->GetComponent<UIOverlayCanvasComponent>() = c;
        else
            entity->AddComponent<UIOverlayCanvasComponent>(c);
        migrateColorToComponent(cj, "color", { 0.08f, 0.08f, 0.10f, 0.6f });
    }
    if (ui.contains("worldCanvas") && ui["worldCanvas"].is_object())
    {
        const auto& cj = ui["worldCanvas"];
        UIWorldCanvasComponent c;
        c.Size        = ReadVec2(cj, "size", { 2.f, 1.f });
        c.Alpha       = cj.value("alpha", 1.f);
        c.DoubleSided = cj.value("doubleSided", true);
        if (entity->HasComponent<UIWorldCanvasComponent>())
            entity->GetComponent<UIWorldCanvasComponent>() = c;
        else
            entity->AddComponent<UIWorldCanvasComponent>(c);
        migrateColorToComponent(cj, "color", { 0.08f, 0.08f, 0.10f, 0.6f });
    }
    if (ui.contains("rectTransform") && ui["rectTransform"].is_object())
    {
        const auto& rj = ui["rectTransform"];
        UIRectTransformComponent r;
        r.AnchorMin = ReadVec2(rj, "anchorMin", { 0.5f, 0.5f });
        r.AnchorMax = ReadVec2(rj, "anchorMax", { 0.5f, 0.5f });
        r.Size      = rj.value("size", 40.f);
        r.Pivot     = ReadVec2(rj, "pivot", { 0.5f, 0.5f });
        r.Alpha     = rj.value("alpha", 1.f);
        r.ZOrder    = rj.value("zOrder", 0);
        if (entity->HasComponent<UIRectTransformComponent>())
            entity->GetComponent<UIRectTransformComponent>() = r;
        else
            entity->AddComponent<UIRectTransformComponent>(r);
    }
    if (ui.contains("parentNode") && ui["parentNode"].is_object())
    {
        const auto& pj = ui["parentNode"];
        ParentNodeComponent p;
        p.Value = UUID::FromString(pj["value"].get<std::string>());
		if (entity->HasComponent<ParentNodeComponent>())
			entity->GetComponent<ParentNodeComponent>() = p;
		else
			entity->AddComponent<ParentNodeComponent>(p);
    }
    if (ui.contains("panel") && ui["panel"].is_object())
    {
        const auto& pj = ui["panel"];
        UIPanelComponent p;
        p.Width        = pj.value("width", 160.f);
        p.BorderColor  = ReadVec4(pj, "borderColor", { .3f,.3f,.35f,1.f });
        p.BorderWidth  = pj.value("borderWidth", 0.f);
        p.CornerRadius = pj.value("cornerRadius", 6.f);
        if (entity->HasComponent<UIPanelComponent>())
            entity->GetComponent<UIPanelComponent>() = p;
        else
            entity->AddComponent<UIPanelComponent>(p);
        migrateColorToComponent(pj, "color", { .1f,.1f,.12f,.9f });
    }
    if (ui.contains("text") && ui["text"].is_object())
    {
        const auto& tj = ui["text"];
        UITextComponent t;
        t.Text            = tj.value("text", "Text");
        t.FontPath        = tj.value("fontPath", "");
        t.FontSize        = tj.value("fontSize", 16.f);
        t.Color           = ReadVec4(tj, "color", { 1,1,1,1 });
        t.HorizontalAlign = static_cast<UITextComponent::HAlign>(tj.value("hAlign", 1));
        t.VerticalAlign   = static_cast<UITextComponent::VAlign>(tj.value("vAlign", 1));
        t.Bold            = tj.value("bold", false);
        t.Italic          = tj.value("italic", false);
        t.WordWrap        = tj.value("wordWrap", true);
        if (entity->HasComponent<UITextComponent>())
            entity->GetComponent<UITextComponent>() = t;
        else
            entity->AddComponent<UITextComponent>(t);
    }
    if (ui.contains("button") && ui["button"].is_object())
    {
        const auto& bj = ui["button"];
        UIButtonComponent b;
        b.Width         = bj.value("width", 160.f);
        b.NormalColor   = ReadVec4(bj, "normalColor",   { 1,1,1,1 });
        b.HoverColor    = ReadVec4(bj, "hoverColor",    { .85f,.9f,1,1 });
        b.PressedColor  = ReadVec4(bj, "pressedColor",  { .65f,.75f,1,1 });
        b.DisabledColor = ReadVec4(bj, "disabledColor", { .5f,.5f,.5f,.6f });
        b.Interactable  = bj.value("interactable", true);
        b.CornerRadius  = bj.value("cornerRadius", 6.f);
        if (entity->HasComponent<UIButtonComponent>())
            entity->GetComponent<UIButtonComponent>() = b;
        else
            entity->AddComponent<UIButtonComponent>(b);
    }
    if (ui.contains("slider") && ui["slider"].is_object())
    {
        const auto& sj = ui["slider"];
        UISliderComponent s;
        s.Width           = sj.value("width", 200.f);
        s.Value           = sj.value("value", 0.5f);
        s.Min             = sj.value("min", 0.f);
        s.Max             = sj.value("max", 1.f);
        s.BackgroundColor = ReadVec4(sj, "bgColor",     { .2f,.2f,.22f,1 });
        s.FillColor       = ReadVec4(sj, "fillColor",   { .04f,.52f,1,1 });
        s.HandleColor     = ReadVec4(sj, "handleColor", { 1,1,1,1 });
        s.HandleSize      = sj.value("handleSize", 16.f);
        s.Interactable    = sj.value("interactable", true);
        if (entity->HasComponent<UISliderComponent>())
            entity->GetComponent<UISliderComponent>() = s;
        else
            entity->AddComponent<UISliderComponent>(s);
    }
    if (ui.contains("image") && ui["image"].is_object())
    {
        const auto& ij = ui["image"];
        UIImageComponent img;
        img.Width          = ij.value("width", 160.f);
        img.TexturePath    = ij.value("texturePath", "");
        img.PreserveAspect = ij.value("preserveAspect", true);
        if (entity->HasComponent<UIImageComponent>())
            entity->GetComponent<UIImageComponent>() = img;
        else
            entity->AddComponent<UIImageComponent>(img);
        migrateColorToComponent(ij, "color", { 1,1,1,1 });
    }
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

    if (!meshComp.Mesh.IsValid()) return;
    const Mesh* m = meshComp.Mesh.operator->();
    if (!m) return;
    for (const auto& matRef : m->GetMaterials())
    {
        if (!matRef) continue;
        MaterialInstance* p = matRef.get();
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

        UUID entityUUID = UUID::FromString(o["uuid"].get<std::string>());
        if (!entityUUID.IsValid()) {
            CHE_CORE_WARN("SceneSerializer: skipping object '{}' with invalid uuid", name);
            continue;
        }

        EntityHandle handle{};
        MeshRef importedMesh;
        bool hasImportedMesh = false;

        if (!meshPath.empty()) {
            MeshLoader* meshLoader = Application::Get().Resources().GetMeshLoader();
            if (ResourceManager::IsPrimitiveUri(meshPath))
            {
                MeshRef primMesh = Application::Get().Resources().LoadPrimitiveMesh(meshPath);
                if (primMesh.IsValid())
                {
                    ShaderHandle sh;
                    if (meshPath == ":primitive:sphere")
                    {
                        sh = Application::Get().Render().GetDefaultSphereImpostorShader();
                        if (!sh.IsValid())
                            sh = Application::Get().Render().GetDefaultMeshShader();
                    }
                    else
                    {
                        sh = Application::Get().Render().GetDefaultMeshShader();
                    }
                    const uint32_t subCount = primMesh->GetSubMeshCount();
                    for (uint32_t i = 0; i < subCount; ++i)
                        meshLoader->SetMaterial(primMesh.Handle(), i,
                            MaterialInstance::FromBase(std::make_shared<Material>(sh)));
                    importedMesh = std::move(primMesh);
                    hasImportedMesh = true;
                }
            }
            else
            {
                auto modelHandle = Application::Get().Resources().Load<ModelHandle>(
                    std::filesystem::path(meshPath), CHEngine::Application::Get().Render().GetDefaultMeshShader());
                const LoadedModel* result = modelHandle.IsValid()
                    ? Application::Get().Resources().GetModel(modelHandle) : nullptr;
                if (result && result->mesh.IsValid()) {
                    importedMesh = result->mesh; // MeshRef copy — AddRefs shared GpuRecord

                    glm::vec3 centroid(0.0f);
                    size_t totalVerts = 0;
                    if (const auto* rec = meshLoader->GetGpuRecord(importedMesh.Handle())) {
                        for (const auto& v : rec->vertices) {
                            centroid += v.Position;
                            ++totalVerts;
                        }
                    }
                    if (totalVerts > 0) centroid /= static_cast<float>(totalVerts);

                    if (totalVerts > 0 && glm::length(centroid) > 1e-5f) {
                        const auto* rec = meshLoader->GetGpuRecord(importedMesh.Handle());
                        if (rec) {
                            auto verts = rec->vertices;
                            for (auto& v : verts) v.Position -= centroid;
                            // Re-create record with shifted geometry; preserve submesh layout + materials.
                            auto subs = rec->subMeshes;
                            auto indicesCopy = rec->indices;
                            std::vector<Ref<MaterialInstance>> mats = importedMesh->GetMaterials();
                            importedMesh = MeshRef{
                                meshLoader->GetOrCreate(verts, indicesCopy, subs, std::move(mats))
                            };
                        }
                    }

                    hasImportedMesh = true;
                } else {
                    std::string normalizedPath = meshPath;
                    std::filesystem::path fsPath(meshPath);
                    if (!fsPath.empty())
                        normalizedPath = fsPath.lexically_normal().string();

                    CHE_CORE_WARN(
                        "SceneSerializer: model load failed for entity='{}' uuid={} meshPath='{}' normalized='{}'",
                        name,
                        entityUUID.ToString(),
                        meshPath,
                        normalizedPath);
                }
            }
        }

        if (!scene->IsEntityHandleValid(handle))
            handle = scene->CreateEntity(name, entityUUID);
        auto* obj = scene->TryGetEntity(handle);
        if (!obj) continue;

        if (!meshPath.empty())
        {
            obj->AddComponent<MeshComponent>();
            obj = scene->TryGetEntity(handle); // re-resolve after potential storage realloc
            if (!obj) continue;
            auto& meshComp = obj->GetComponent<MeshComponent>();
            if (hasImportedMesh)
                meshComp.Mesh = std::move(importedMesh);
            meshComp.SourcePath = meshPath;
        }

        if (o.contains("position") || o.contains("rotation") || o.contains("scale"))
        {
            obj->AddComponent<TransformComponent>();
            obj = scene->TryGetEntity(handle);
            if (!obj) continue;
        }

        if (o.contains("color"))
        {
            obj->AddComponent<ColorComponent>();
            obj = scene->TryGetEntity(handle);
            if (!obj) continue;
        }

        auto& visible = obj->GetComponent<VisibilityComponent>().Visible;

        float v3[3];
        if (obj->HasComponent<TransformComponent>())
        {
            auto& transform = obj->GetComponent<TransformComponent>().ObjectTransform;
            if (o.contains("position") && readFloats(o["position"], 3, v3))
                transform.Position = { v3[0], v3[1], v3[2] };
            if (o.contains("rotation") && readFloats(o["rotation"], 3, v3))
                transform.Rotation = { v3[0], v3[1], v3[2] };
            if (o.contains("scale") && readFloats(o["scale"], 3, v3))
                transform.Scale = { v3[0], v3[1], v3[2] };
        }

        float v4[4];
        if (obj->HasComponent<ColorComponent>() && o.contains("color") && readFloats(o["color"], 4, v4))
            obj->GetComponent<ColorComponent>().Color = { v4[0], v4[1], v4[2], v4[3] };

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

            // 0 = Perspective, 1 = Orthographic
            const int projectionType = cameraJson.value("projectionType", 0);
            if (projectionType == 1) {
                OrthographicCamera ortho;
                if (cameraJson.contains("orthographic") && cameraJson["orthographic"].is_object()) {
                    const auto& o_json = cameraJson["orthographic"];
                    ortho.SetOrthographic(
                        o_json.value("size",     ortho.GetSize()),
                        o_json.value("nearClip", ortho.GetNearClip()),
                        o_json.value("farClip",  ortho.GetFarClip()));
                }
                ortho.SetViewportSize(static_cast<uint32_t>(safeAspectRatio * 1000.0f), 1000u);
                cameraComp->Camera = std::move(ortho);
            } else {
                PerspectiveCamera persp;
                if (cameraJson.contains("perspective") && cameraJson["perspective"].is_object()) {
                    const auto& p_json = cameraJson["perspective"];
                    persp.SetPerspective(
                        p_json.value("verticalFOV", persp.GetVerticalFOV()),
                        p_json.value("nearClip",    persp.GetNearClip()),
                        p_json.value("farClip",     persp.GetFarClip()));
                } else if (cameraJson.contains("fov")) {
                    // Legacy fallback (v2/v3): old scalar fov-in-degrees.
                    persp.SetPerspective(
                        glm::radians(cameraJson.value("fov", 45.0f)),
                        cameraJson.value("nearClip", 0.1f),
                        cameraJson.value("farClip", 1000.0f));
                }
                persp.SetViewportSize(static_cast<uint32_t>(safeAspectRatio * 1000.0f), 1000u);
                cameraComp->Camera = std::move(persp);
            }
        }

        if (obj->HasComponent<MeshComponent>())
        {
            MeshLoader* meshLoader = Application::Get().Resources().GetMeshLoader();
            auto* meshCompForMat = &obj->GetComponent<MeshComponent>();
            if (meshCompForMat->Mesh.IsValid())
            {
                const Mesh* m = meshCompForMat->Mesh.operator->();
                const uint32_t subCount = m ? m->GetSubMeshCount() : 0;
                for (uint32_t i = 0; i < subCount; ++i)
                {
                    if (!m->GetMaterial(i))
                        meshLoader->SetMaterial(meshCompForMat->Mesh.Handle(), i,
                            MaterialInstance::FromBase(std::make_shared<Material>(
                                CHEngine::Application::Get().Render().GetDefaultMeshShader())));
                }

                if (o.contains("materials") && o["materials"].is_array())
                {
                    const auto& arr = o["materials"];
                    for (size_t mi = 0; mi < arr.size() && mi < subCount; ++mi)
                    {
                        if (arr[mi].is_object())
                        {
                            auto mat = m->GetMaterial(static_cast<uint32_t>(mi));
                            if (mat) ApplyMaterialFromJson(arr[mi], *mat);
                        }
                    }
                }
                else if (o.contains("material") && o["material"].is_object() && subCount > 0)
                {
                    auto mat0 = m->GetMaterial(0);
                    if (mat0)
                    {
                        ApplyMaterialFromJson(o["material"], *mat0);
                        for (uint32_t i = 1; i < subCount; ++i)
                            meshLoader->SetMaterial(meshCompForMat->Mesh.Handle(), i, mat0);
                    }
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

        if (o.contains("ui") && o["ui"].is_object())
            DeserializeUIComponents(obj, o["ui"]);
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
        o["uuid"] = uuid.ToString();
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
            const Mesh* mWrite = meshComp.Mesh.IsValid() ? meshComp.Mesh.operator->() : nullptr;
            const uint32_t subCountWrite = mWrite ? mWrite->GetSubMeshCount() : 0;
            for (uint32_t mi = 0; mi < subCountWrite; ++mi)
            {
                json matObj;
                Ref<MaterialInstance> mat = mWrite->GetMaterial(mi);
                if (mat) {
                    matObj["diffusePath"]   = mat->EffectiveDiffuseMapPath();
                    matObj["specularPath"]  = mat->EffectiveSpecularMapPath();
                    matObj["shininess"]     = mat->Shininess;
                    matObj["specularScale"] = mat->SpecularScale;
                    matObj["roughness"]     = mat->Roughness;
                    matObj["metallic"]      = mat->Metallic;
                    matObj["ao"]            = mat->AO;
                    matObj["usePBR"]        = mat->GetMaterial()
                                              ? (mat->GetMaterial()->MaterialFlags & kPBR_EnablePBR) != 0
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
            std::visit([&](const auto& c) {
                using T = std::decay_t<decltype(c)>;
                if constexpr (std::is_same_v<T, PerspectiveCamera>) {
                    camera["projectionType"] = 0;
                    camera["perspective"] = {
                        { "verticalFOV", c.GetVerticalFOV() },
                        { "nearClip",    c.GetNearClip() },
                        { "farClip",     c.GetFarClip() }
                    };
                } else if constexpr (std::is_same_v<T, OrthographicCamera>) {
                    camera["projectionType"] = 1;
                    camera["orthographic"] = {
                        { "size",     c.GetSize() },
                        { "nearClip", c.GetNearClip() },
                        { "farClip",  c.GetFarClip() }
                    };
                }
            }, cameraComp->Camera);
            camera["aspectRatio"]      = 16.0f / 9.0f;
            camera["fixedAspectRatio"] = cameraComp->FixedAspectRatio;
            camera["primary"]          = cameraComp->Primary;
            o["camera"] = camera;
        }

        {
            json ui = SerializeUIComponents(entity);
            if (!ui.empty()) o["ui"] = ui;
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
        o["uuid"] = uuid.ToString();
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
            const Mesh* mWrite = meshComp.Mesh.IsValid() ? meshComp.Mesh.operator->() : nullptr;
            const uint32_t subCountWrite = mWrite ? mWrite->GetSubMeshCount() : 0;
            for (uint32_t mi = 0; mi < subCountWrite; ++mi)
            {
                json matObj;
                Ref<MaterialInstance> mat = mWrite->GetMaterial(mi);
                if (mat) {
                    matObj["diffusePath"]   = mat->EffectiveDiffuseMapPath();
                    matObj["specularPath"]  = mat->EffectiveSpecularMapPath();
                    matObj["shininess"]     = mat->Shininess;
                    matObj["specularScale"] = mat->SpecularScale;
                    matObj["roughness"]     = mat->Roughness;
                    matObj["metallic"]      = mat->Metallic;
                    matObj["ao"]            = mat->AO;
                    matObj["usePBR"]        = mat->GetMaterial()
                                              ? (mat->GetMaterial()->MaterialFlags & kPBR_EnablePBR) != 0
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
            std::visit([&](const auto& c) {
                using T = std::decay_t<decltype(c)>;
                if constexpr (std::is_same_v<T, PerspectiveCamera>) {
                    camera["projectionType"] = 0;
                    camera["perspective"] = {
                        { "verticalFOV", c.GetVerticalFOV() },
                        { "nearClip",    c.GetNearClip() },
                        { "farClip",     c.GetFarClip() }
                    };
                } else if constexpr (std::is_same_v<T, OrthographicCamera>) {
                    camera["projectionType"] = 1;
                    camera["orthographic"] = {
                        { "size",     c.GetSize() },
                        { "nearClip", c.GetNearClip() },
                        { "farClip",  c.GetFarClip() }
                    };
                }
            }, cameraComp->Camera);
            camera["aspectRatio"]      = 16.0f / 9.0f;
            camera["fixedAspectRatio"] = cameraComp->FixedAspectRatio;
            camera["primary"]          = cameraComp->Primary;
            o["camera"] = camera;
        }

        {
            json ui = SerializeUIComponents(entity);
            if (!ui.empty()) o["ui"] = ui;
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
