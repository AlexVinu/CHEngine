#include "EditorWorldContext.h"

#include <CHEngine/Application.h>
#include <CHEngine/World/World.h>
#include <CHEngine/Mesh/Material.h>
#include <CHEngine/ResourceManager/ResourceManager.h>
#include <CHEngine/Scene/Components.h>
#include <CHEngine/Camera/PerspectiveCamera.h>
#include <Log/Log.h>

#include <glm/glm.hpp>

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <vector>

EditorWorldContext::EditorWorldContext(CHEngine::WorldsList* list)
    : SceneSession(list)
{
	UpdateState(std::nullopt, std::nullopt);
}

void EditorWorldContext::UpdateState(std::optional<SceneSession::State> new_state, std::optional<bool> is_active)
{
	CHE_CORE_ASSERT(EditorScene, "EditorWorldContext must have EditorScene");
    m_IsActive = is_active.value_or(m_IsActive);
	m_SessionState = new_state.value_or(m_SessionState);

	// Ensure scene is set when becoming active
	if (m_IsActive && !GetSceneRef())
	{
		ActivateEditorScene();
	}

	switch (m_SessionState)
	{
	case SceneSession::State::Edit:
		if (!m_IsActive)
		{
			SetState(CHEngine::WorldState::NONE);
			return;
		}
		SetState(CHEngine::WorldState::Presenting);
		SetActiveCamera(ViewportCamera.get());
		break;

	case SceneSession::State::Play:
		if (m_IsActive)
		{
			SetState(CHEngine::WorldState::Simulating);
			SetActiveCamera(nullptr);
		}
		else
			SetState(CHEngine::WorldState::SimulatingWithoutPresenting);

		break;

	case SceneSession::State::Pause:
		if (!m_IsActive) return;
		SetState(CHEngine::WorldState::Presenting);
		SetActiveCamera(nullptr);
		break;
	}
}

void EditorWorldContext::ActivateActiveScene()
{
    CHE_ASSERT(ActiveScene, "THERE ARE NO ACTIVE SCENE");
    CHE_ASSERT(EditorScene, "THERE ARE NO EDITOR SCENE");

    *ActiveScene = *EditorScene;
    SetScene(ActiveScene);
}

void EditorWorldContext::ActivateEditorScene()
{
    CHE_ASSERT(ActiveScene, "THERE ARE NO ACTIVE SCENE");
    CHE_ASSERT(EditorScene, "THERE ARE NO EDITOR SCENE");

    *ActiveScene = *EditorScene;
    SetScene(EditorScene);
}

// ── Scene authoring commands ─────────────────────────────────────────────────

namespace {

// Collect handles by name first, then call AddComponent outside ForEach.
// Entity* from TryGetEntity can dangle after AddComponent (entt realloc); handles are stable.
std::vector<CHEngine::EntityHandle> FindHandlesByName(CHEngine::Scene* scene,
                                                      const std::string& name)
{
    std::vector<CHEngine::EntityHandle> result;
    if (!scene || name.empty()) return result;
    scene->ForEach<CHEngine::TagComponent>(
        [&](CHEngine::EntityHandle h, const CHEngine::UUID&, CHEngine::TagComponent& tag) {
            if (scene->GetString(tag.Name) == name)
                result.push_back(h);
        });
    return result;
}

// Resolve a single entity handle by name through the engine's name index
// (GetUUIDByString → handle). Returns an invalid handle if not found.
CHEngine::EntityHandle FindHandleByName(CHEngine::Scene* scene, const std::string& name)
{
    if (!scene || name.empty()) return {};
    return scene->TryGetEntityHandleByUUID(scene->GetUUIDByString(name));
}

} // namespace

void EditorWorldContext::DestroyEntityByUuid(const CHEngine::UUID& object_id)
{
    auto scene_ref = EditorScene;
    if (!scene_ref)
        return;
    if (scene_ref->IsEntityHandleValid(SelectedEntity)
        && scene_ref->GetUUID(SelectedEntity) == object_id)
        SelectedEntity = {};
    scene_ref->DestroyEntity(object_id);
}

void EditorWorldContext::AddDirectionalLight()
{
    auto scene_ref = EditorScene;
    if (!scene_ref)
        return;

    const CHEngine::EntityHandle handle = scene_ref->CreateEntity("Directional Light");
    if (auto* entity = scene_ref->TryGetEntity(handle); entity)
    {
        entity->AddComponent<CHEngine::TransformComponent>();
        entity->AddComponent<CHEngine::LightComponent>(
            CHEngine::LightComponent{ CHEngine::Light{ CHEngine::LightType::Directional } });
        entity->PatchComponent<CHEngine::TransformComponent>([](CHEngine::TransformComponent& transform_component) {
            transform_component.ObjectTransform.Rotation = { -45.0f, -30.0f, 0.0f };
            transform_component.MarkDirty();
        });
        SelectedEntity = handle;
    }
}

void EditorWorldContext::AddPointLight()
{
    auto scene_ref = EditorScene;
    if (!scene_ref)
        return;

    const CHEngine::EntityHandle handle = scene_ref->CreateEntity("Point Light");
    if (auto* entity = scene_ref->TryGetEntity(handle); entity)
    {
        entity->AddComponent<CHEngine::TransformComponent>();
        entity->AddComponent<CHEngine::LightComponent>(
            CHEngine::LightComponent{ CHEngine::Light{ CHEngine::LightType::Point } });
        entity->PatchComponent<CHEngine::TransformComponent>([](CHEngine::TransformComponent& transform_component) {
            transform_component.ObjectTransform.Position = { 0.0f, 3.0f, 0.0f };
            transform_component.MarkDirty();
        });
        SelectedEntity = handle;
    }
}

void EditorWorldContext::AddSpotLight()
{
    auto scene_ref = EditorScene;
    if (!scene_ref)
        return;

    const CHEngine::EntityHandle handle = scene_ref->CreateEntity("Spot Light");
    if (auto* entity = scene_ref->TryGetEntity(handle); entity)
    {
        entity->AddComponent<CHEngine::TransformComponent>();
        entity->AddComponent<CHEngine::LightComponent>(
            CHEngine::LightComponent{ CHEngine::Light{ CHEngine::LightType::Spot } });
        entity->PatchComponent<CHEngine::TransformComponent>([](CHEngine::TransformComponent& transform_component) {
            transform_component.ObjectTransform.Position = { 0.0f, 5.0f, 0.0f };
            transform_component.ObjectTransform.Rotation = { -90.0f, 0.0f, 0.0f };
            transform_component.MarkDirty();
        });
        SelectedEntity = handle;
    }
}

void EditorWorldContext::AddCube(CHEngine::ShaderHandle meshShader)
{
    auto scene_ref = EditorScene;
    if (!scene_ref)
        return;

    const CHEngine::EntityHandle handle = scene_ref->CreateEntity("Cube");
    auto* entity = scene_ref->TryGetEntity(handle);
    if (!entity)
        return;

    entity->AddComponent<CHEngine::TransformComponent>();
    entity->AddComponent<CHEngine::MeshComponent>();

    CHEngine::MeshRef cube_mesh = CHEngine::Application::Get().Resources().LoadPrimitiveMesh(":primitive:cube");
    CHEngine::Application::Get().Resources().GetMeshLoader()->SetMaterial(cube_mesh.Handle(), 0,
        CHEngine::MaterialInstance::FromBase(
            std::make_shared<CHEngine::Material>(meshShader)));
    entity->PatchComponent<CHEngine::MeshComponent>(
        [cube_mesh = std::move(cube_mesh), &scene_ref](CHEngine::MeshComponent& mesh_component) mutable {
            mesh_component.Mesh = std::move(cube_mesh);
            mesh_component.SourcePath = scene_ref->InternString(":primitive:cube");
        });

    SelectedEntity = handle;
}

void EditorWorldContext::AddSphere(CHEngine::ShaderHandle impostorShader)
{
    auto scene_ref = EditorScene;
    if (!scene_ref)
        return;

    const CHEngine::EntityHandle handle = scene_ref->CreateEntity("Sphere");
    auto* entity = scene_ref->TryGetEntity(handle);
    if (!entity)
        return;

    entity->AddComponent<CHEngine::TransformComponent>();
    entity->AddComponent<CHEngine::MeshComponent>();

    // Sphere impostor: 2 triangles + ray-sphere intersection in shader.
    // World radius is encoded as the entity's transform scale (scale == radius).
    CHEngine::MeshRef sphere_mesh = CHEngine::Application::Get().Resources().LoadPrimitiveMesh(":primitive:sphere");
    CHEngine::Application::Get().Resources().GetMeshLoader()->SetMaterial(sphere_mesh.Handle(), 0,
        CHEngine::MaterialInstance::FromBase(
            std::make_shared<CHEngine::Material>(impostorShader)));
    entity->PatchComponent<CHEngine::MeshComponent>(
        [sphere_mesh = std::move(sphere_mesh), &scene_ref](CHEngine::MeshComponent& mesh_component) mutable {
            mesh_component.Mesh = std::move(sphere_mesh);
            mesh_component.SourcePath = scene_ref->InternString(":primitive:sphere");
        });

    SelectedEntity = handle;
}

void EditorWorldContext::AddCameraEntity()
{
    auto scene_ref = EditorScene;
    if (!scene_ref)
        return;


    const CHEngine::EntityHandle handle = scene_ref->CreateEntity("Camera");
    auto* entity = scene_ref->TryGetEntity(handle);
    if (!entity)
        return;

    entity->AddComponent<CHEngine::TransformComponent>();
    entity->AddComponent<CHEngine::CameraComponent>();
    SelectedEntity = handle;
}

void EditorWorldContext::AddEmptyEntity()
{
    auto scene_ref = EditorScene;
    if (!scene_ref)
        return;

    const CHEngine::EntityHandle handle = scene_ref->CreateEntity("New Object");
    if (auto* entity = scene_ref->TryGetEntity(handle); entity)
        entity->AddComponent<CHEngine::TransformComponent>();
    SelectedEntity = handle;
}

void EditorWorldContext::AddUIOverlayCanvas()
{
    auto scene = EditorScene;
    if (!scene) return;
    auto h = scene->CreateEntity("UI Overlay Canvas", CHEngine::UUID::Generate());
    auto* e = scene->TryGetEntity(h);
    if (!e) return;
    e->AddComponent<CHEngine::UIOverlayCanvasComponent>();
    e->AddComponent<CHEngine::ColorComponent>(CHEngine::ColorComponent{ { 0.08f, 0.08f, 0.10f, 0.6f } });
    SelectedEntity = h;
}

void EditorWorldContext::AddUIWorldCanvas()
{
    auto scene = EditorScene;
    if (!scene) return;
    auto h = scene->CreateEntity("UI World Canvas", CHEngine::UUID::Generate());
    auto* e = scene->TryGetEntity(h);
    if (!e) return;
    e->AddComponent<CHEngine::TransformComponent>();
    e->AddComponent<CHEngine::UIWorldCanvasComponent>();
    e->AddComponent<CHEngine::ColorComponent>(CHEngine::ColorComponent{ { 0.08f, 0.08f, 0.10f, 0.6f } });
    SelectedEntity = h;
}

void EditorWorldContext::AddUIPanel(const CHEngine::UUID& canvas)
{
    auto scene = EditorScene;
    if (!scene) return;
    auto h = scene->CreateEntity("UI Panel", CHEngine::UUID::Generate());
    auto* e = scene->TryGetEntity(h);
    if (!e) return;
    e->AddComponent<CHEngine::UIPanelComponent>();
    e->AddComponent<CHEngine::ColorComponent>(CHEngine::ColorComponent{ { 0.10f, 0.10f, 0.12f, 0.90f } });
    e->AddComponent<CHEngine::ParentNodeComponent>(canvas);
    e->AddComponent<CHEngine::UIRectTransformComponent>();
    SelectedEntity = h;
}

void EditorWorldContext::AddUIText(const CHEngine::UUID& canvas)
{
    auto scene = EditorScene;
    if (!scene) return;
    auto h = scene->CreateEntity("UI Text", CHEngine::UUID::Generate());
    auto* e = scene->TryGetEntity(h);
    if (!e) return;
    e->AddComponent<CHEngine::UITextComponent>();
    e->AddComponent<CHEngine::ColorComponent>(CHEngine::ColorComponent{ { 0.10f, 0.10f, 0.12f, 0.90f } });
    e->AddComponent<CHEngine::ParentNodeComponent>(canvas);
    e->AddComponent<CHEngine::UIRectTransformComponent>();
    SelectedEntity = h;
}

void EditorWorldContext::AddUIButton(const CHEngine::UUID& canvas)
{
    auto scene = EditorScene;
    if (!scene) return;
    auto h = scene->CreateEntity("UI Button", CHEngine::UUID::Generate());
    auto* e = scene->TryGetEntity(h);
    if (!e) return;
    e->AddComponent<CHEngine::UIButtonComponent>();
    e->AddComponent<CHEngine::ColorComponent>(CHEngine::ColorComponent{ { 0.10f, 0.10f, 0.12f, 0.90f } });
    e->AddComponent<CHEngine::ParentNodeComponent>(canvas);
    e->AddComponent<CHEngine::UIRectTransformComponent>();
    SelectedEntity = h;
}

void EditorWorldContext::AddUIImage(const CHEngine::UUID& canvas)
{
    auto scene = EditorScene;
    if (!scene) return;
    auto h = scene->CreateEntity("UI Image", CHEngine::UUID::Generate());
    auto* e = scene->TryGetEntity(h);
    if (!e) return;
    e->AddComponent<CHEngine::UIImageComponent>();
    e->AddComponent<CHEngine::ColorComponent>(CHEngine::ColorComponent{ { 0.10f, 0.10f, 0.12f, 0.90f } });
    e->AddComponent<CHEngine::ParentNodeComponent>(canvas);
    e->AddComponent<CHEngine::UIRectTransformComponent>();
    SelectedEntity = h;
}

void EditorWorldContext::AddUISlider(const CHEngine::UUID& canvas)
{
    auto scene = EditorScene;
    if (!scene) return;
    auto h = scene->CreateEntity("UI Slider", CHEngine::UUID::Generate());
    auto* e = scene->TryGetEntity(h);
    if (!e) return;
    e->AddComponent<CHEngine::UISliderComponent>();
    e->AddComponent<CHEngine::ColorComponent>(CHEngine::ColorComponent{ { 0.10f, 0.10f, 0.12f, 0.90f } });
    e->AddComponent<CHEngine::ParentNodeComponent>(canvas);
    e->AddComponent<CHEngine::UIRectTransformComponent>();
    SelectedEntity = h;
}

void EditorWorldContext::SetSelectedPosition(float x, float y, float z)
{
    auto scene = EditorScene;
    if (!scene) return;
    auto* entity = scene->TryGetEntity(SelectedEntity);
    if (!entity || !entity->HasComponent<CHEngine::TransformComponent>()) return;
    entity->PatchComponent<CHEngine::TransformComponent>(
        [x, y, z](CHEngine::TransformComponent& tc) {
            tc.ObjectTransform.Position = { x, y, z };
            tc.MarkDirty();
        });
}

void EditorWorldContext::SetSelectedRotation(float x, float y, float z)
{
    auto scene = EditorScene;
    if (!scene) return;
    auto* e = scene->TryGetEntity(SelectedEntity);
    if (!e || !e->HasComponent<CHEngine::TransformComponent>()) return;
    e->PatchComponent<CHEngine::TransformComponent>(
        [x,y,z](CHEngine::TransformComponent& tc) { tc.ObjectTransform.Rotation = {x,y,z}; tc.MarkDirty(); });
}

void EditorWorldContext::SetSelectedScale(float x, float y, float z)
{
    auto scene = EditorScene;
    if (!scene) return;
    auto* e = scene->TryGetEntity(SelectedEntity);
    if (!e || !e->HasComponent<CHEngine::TransformComponent>()) return;
    e->PatchComponent<CHEngine::TransformComponent>(
        [x,y,z](CHEngine::TransformComponent& tc) { tc.ObjectTransform.Scale = {x,y,z}; tc.MarkDirty(); });
}

void EditorWorldContext::RenameSelected(const std::string& newName)
{
    auto scene = EditorScene;
    if (!scene || newName.empty()) return;
    auto* e = scene->TryGetEntity(SelectedEntity);
    if (!e || !e->HasComponent<CHEngine::TagComponent>()) return;
    e->PatchComponent<CHEngine::TagComponent>(
        [&, scenePtr = scene.get()](CHEngine::TagComponent& tag) {
            tag.Name = scenePtr->InternString(newName);
        });
}

void EditorWorldContext::SelectByName(const std::string& name)
{
    if (name.empty()) return;
    auto scene = EditorScene;
    if (!scene) return;

    const CHEngine::EntityHandle found = FindHandleByName(scene.get(), name);
    if (scene->IsEntityHandleValid(found))
        SelectedEntity = found;
}

void EditorWorldContext::SetPositionByName(const std::string& name, float x, float y, float z)
{
    auto* scene0 = EditorScene.get();
    for (auto h : FindHandlesByName(scene0, name))
        if (auto* e = scene0->TryGetEntity(h); e && e->HasComponent<CHEngine::TransformComponent>())
            e->PatchComponent<CHEngine::TransformComponent>(
                [x,y,z](CHEngine::TransformComponent& tc) { tc.ObjectTransform.Position = {x,y,z}; tc.MarkDirty(); });
}

void EditorWorldContext::SetRotationByName(const std::string& name, float x, float y, float z)
{
    auto* scene1 = EditorScene.get();
    for (auto h : FindHandlesByName(scene1, name))
        if (auto* e = scene1->TryGetEntity(h); e && e->HasComponent<CHEngine::TransformComponent>())
            e->PatchComponent<CHEngine::TransformComponent>(
                [x,y,z](CHEngine::TransformComponent& tc) { tc.ObjectTransform.Rotation = {x,y,z}; tc.MarkDirty(); });
}

void EditorWorldContext::SetScaleByName(const std::string& name, float x, float y, float z)
{
    auto* scene2 = EditorScene.get();
    for (auto h : FindHandlesByName(scene2, name))
        if (auto* e = scene2->TryGetEntity(h); e && e->HasComponent<CHEngine::TransformComponent>())
            e->PatchComponent<CHEngine::TransformComponent>(
                [x,y,z](CHEngine::TransformComponent& tc) { tc.ObjectTransform.Scale = {x,y,z}; tc.MarkDirty(); });
}

void EditorWorldContext::SetColorByName(const std::string& name, float r, float g, float b, float a)
{
    // Two-phase: collect first, then AddComponent (safe outside ForEach)
    auto* scene3 = EditorScene.get();
    for (auto h : FindHandlesByName(scene3, name)) {
        auto* e = scene3->TryGetEntity(h);
        if (!e) continue;
        if (!e->HasComponent<CHEngine::ColorComponent>())
            e->AddComponent<CHEngine::ColorComponent>();
        // Re-resolve after AddComponent (may have reallocated storage)
        e = scene3->TryGetEntity(h);
        if (e) e->PatchComponent<CHEngine::ColorComponent>(
            [r,g,b,a](CHEngine::ColorComponent& cc) { cc.Color = {r,g,b,a}; });
    }
}

void EditorWorldContext::SetVisibleByName(const std::string& name, bool visible)
{
    auto* scene4 = EditorScene.get();
    for (auto h : FindHandlesByName(scene4, name)) {
        auto* e = scene4->TryGetEntity(h);
        if (!e) continue;
        if (!e->HasComponent<CHEngine::VisibilityComponent>())
            e->AddComponent<CHEngine::VisibilityComponent>();
        e = scene4->TryGetEntity(h);
        if (e) e->PatchComponent<CHEngine::VisibilityComponent>(
            [visible](CHEngine::VisibilityComponent& vc) { vc.Visible = visible; });
    }
}

void EditorWorldContext::SetLightByName(const std::string& name, const std::string& lightType,
                                        float r, float g, float b, float intensity)
{
    CHEngine::LightType ltype = CHEngine::LightType::Directional;
    if (lightType == "point") ltype = CHEngine::LightType::Point;
    if (lightType == "spot")  ltype = CHEngine::LightType::Spot;

    auto* scene6 = EditorScene.get();
    for (auto h : FindHandlesByName(scene6, name)) {
        auto* e = scene6->TryGetEntity(h);
        if (!e) continue;
        if (!e->HasComponent<CHEngine::LightComponent>())
            e->AddComponent<CHEngine::LightComponent>();
        e = scene6->TryGetEntity(h);
        if (e) e->PatchComponent<CHEngine::LightComponent>(
            [ltype, r, g, b, intensity](CHEngine::LightComponent& lc) {
                lc.LightData.Type      = ltype;
                lc.LightData.Color     = { r, g, b };
                lc.LightData.Intensity = intensity;
            });
    }
}

void EditorWorldContext::RenameByName(const std::string& oldName, const std::string& newName)
{
    if (newName.empty()) return;
    auto* scene5 = EditorScene.get();
    for (auto h : FindHandlesByName(scene5, oldName))
        if (auto* e = scene5->TryGetEntity(h); e && e->HasComponent<CHEngine::TagComponent>())
            e->PatchComponent<CHEngine::TagComponent>(
                [&, scene5](CHEngine::TagComponent& tag) { tag.Name = scene5->InternString(newName); });
}

void EditorWorldContext::DeleteByName(const std::string& name)
{
    auto scene = EditorScene;
    if (!scene || name.empty()) return;

    const CHEngine::UUID target_uuid = scene->GetUUIDByString(name);
    if (target_uuid != CHEngine::UUID::Nil())
        DestroyEntityByUuid(target_uuid);
}

void EditorWorldContext::ApplyDiffuseTexture(size_t submesh_index, const std::string& filepath,
                                             CHEngine::ShaderHandle meshShader)
{
    CHEngine::Scene* scene = EditorScene.get();
    if (!scene || !scene->IsEntityHandleValid(SelectedEntity))
        return;
    auto* selectedEntity = scene->TryGetEntity(SelectedEntity);
    if (!selectedEntity || !selectedEntity->HasComponent<CHEngine::MeshComponent>())
        return;
    const auto& meshComp = selectedEntity->GetComponent<CHEngine::MeshComponent>();
    if (!meshComp.Mesh.IsValid() || submesh_index >= meshComp.Mesh->GetSubMeshCount())
        return;
    selectedEntity->PatchComponent<CHEngine::MeshComponent>([&](CHEngine::MeshComponent& mesh_component) {
        auto& subMesh = mesh_component.Mesh;
        CHEngine::MeshLoader* ml = CHEngine::Application::Get().Resources().GetMeshLoader();
        if (!subMesh->GetMaterial(static_cast<uint32_t>(submesh_index)))
            ml->SetMaterial(subMesh.Handle(), static_cast<uint32_t>(submesh_index),
                CHEngine::MaterialInstance::FromBase(
                    std::make_shared<CHEngine::Material>(meshShader)));
        auto mat_ref = subMesh->GetMaterial(static_cast<uint32_t>(submesh_index));
        CHEngine::TextureHandle d0, s0;
        mat_ref->ResolveTextures(d0, s0);
        if (d0.IsValid())
            CHEngine::Application::Get().Resources().Unload(d0);
        if (mat_ref->m_Material)
        {
            mat_ref->m_Material->DiffuseMap = CHEngine::TextureHandle{};
            mat_ref->m_Material->DiffuseMapPath.clear();
        }
        mat_ref->DiffuseMap = CHEngine::Application::Get().Resources().Load<CHEngine::TextureHandle>(filepath);
        mat_ref->DiffuseMapPath = mat_ref->DiffuseMap.IsValid() ? filepath : "";
    });
}

void EditorWorldContext::ClearDiffuseTexture(size_t submesh_index)
{
    CHEngine::Scene* scene = EditorScene.get();
    if (!scene || !scene->IsEntityHandleValid(SelectedEntity))
        return;
    auto* selectedEntity = scene->TryGetEntity(SelectedEntity);
    if (!selectedEntity || !selectedEntity->HasComponent<CHEngine::MeshComponent>())
        return;
    const auto& meshComp = selectedEntity->GetComponent<CHEngine::MeshComponent>();
    if (!meshComp.Mesh.IsValid() || submesh_index >= meshComp.Mesh->GetSubMeshCount())
        return;
    selectedEntity->PatchComponent<CHEngine::MeshComponent>([&](CHEngine::MeshComponent& mesh_component) {
        auto& subMesh = mesh_component.Mesh;
        auto mat_ref = subMesh.IsValid() ? subMesh->GetMaterial(static_cast<uint32_t>(submesh_index)) : nullptr;
        if (!mat_ref)
            return;
        CHEngine::TextureHandle d0, s0;
        mat_ref->ResolveTextures(d0, s0);
        if (d0.IsValid())
            CHEngine::Application::Get().Resources().Unload(d0);
        mat_ref->DiffuseMap = CHEngine::TextureHandle{};
        mat_ref->DiffuseMapPath.clear();
        if (mat_ref->m_Material)
        {
            mat_ref->m_Material->DiffuseMap = CHEngine::TextureHandle{};
            mat_ref->m_Material->DiffuseMapPath.clear();
        }
    });
}

void EditorWorldContext::ApplySpecularTexture(size_t submesh_index, const std::string& filepath,
                                              CHEngine::ShaderHandle meshShader)
{
    CHEngine::Scene* scene = EditorScene.get();
    if (!scene || !scene->IsEntityHandleValid(SelectedEntity))
        return;
    auto* selectedEntity = scene->TryGetEntity(SelectedEntity);
    if (!selectedEntity || !selectedEntity->HasComponent<CHEngine::MeshComponent>())
        return;
    const auto& meshComp = selectedEntity->GetComponent<CHEngine::MeshComponent>();
    if (!meshComp.Mesh.IsValid() || submesh_index >= meshComp.Mesh->GetSubMeshCount())
        return;
    selectedEntity->PatchComponent<CHEngine::MeshComponent>([&](CHEngine::MeshComponent& mesh_component) {
        auto& subMesh = mesh_component.Mesh;
        CHEngine::MeshLoader* ml = CHEngine::Application::Get().Resources().GetMeshLoader();
        if (!subMesh->GetMaterial(static_cast<uint32_t>(submesh_index)))
            ml->SetMaterial(subMesh.Handle(), static_cast<uint32_t>(submesh_index),
                CHEngine::MaterialInstance::FromBase(
                    std::make_shared<CHEngine::Material>(meshShader)));
        auto mat_ref = subMesh->GetMaterial(static_cast<uint32_t>(submesh_index));
        CHEngine::TextureHandle d0, s0;
        mat_ref->ResolveTextures(d0, s0);
        if (s0.IsValid())
            CHEngine::Application::Get().Resources().Unload(s0);
        if (mat_ref->m_Material)
        {
            mat_ref->m_Material->SpecularMap = CHEngine::TextureHandle{};
            mat_ref->m_Material->SpecularMapPath.clear();
        }
        mat_ref->SpecularMap = CHEngine::Application::Get().Resources().Load<CHEngine::TextureHandle>(filepath);
        mat_ref->SpecularMapPath = mat_ref->SpecularMap.IsValid() ? filepath : "";
    });
}

void EditorWorldContext::ClearSpecularTexture(size_t submesh_index)
{
    CHEngine::Scene* scene = EditorScene.get();
    if (!scene || !scene->IsEntityHandleValid(SelectedEntity))
        return;
    auto* selectedEntity = scene->TryGetEntity(SelectedEntity);
    if (!selectedEntity || !selectedEntity->HasComponent<CHEngine::MeshComponent>())
        return;
    const auto& meshComp = selectedEntity->GetComponent<CHEngine::MeshComponent>();
    if (!meshComp.Mesh.IsValid() || submesh_index >= meshComp.Mesh->GetSubMeshCount())
        return;
    selectedEntity->PatchComponent<CHEngine::MeshComponent>([&](CHEngine::MeshComponent& mesh_component) {
        auto& subMesh = mesh_component.Mesh;
        auto mat_ref = subMesh.IsValid() ? subMesh->GetMaterial(static_cast<uint32_t>(submesh_index)) : nullptr;
        if (!mat_ref)
            return;
        CHEngine::TextureHandle d0, s0;
        mat_ref->ResolveTextures(d0, s0);
        if (s0.IsValid())
            CHEngine::Application::Get().Resources().Unload(s0);
        mat_ref->SpecularMap = CHEngine::TextureHandle{};
        mat_ref->SpecularMapPath.clear();
        if (mat_ref->m_Material)
        {
            mat_ref->m_Material->SpecularMap = CHEngine::TextureHandle{};
            mat_ref->m_Material->SpecularMapPath.clear();
        }
    });
}

void EditorWorldContext::SetViewportFov(float fov_degrees)
{
    auto* cam = ViewportCamera.get();
    if (!cam)
        return;
    auto& camVariant = cam->GetCamera();
    if (auto* p = std::get_if<CHEngine::PerspectiveCamera>(&camVariant))
        p->SetVerticalFOV(glm::radians(fov_degrees));
}

std::string EditorWorldContext::GetSceneContextString() const
{
    auto scene = EditorScene;

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1);

    ss << "CURRENT SCENE ENTITIES:\n";

    if (!scene)
    {
        ss << "  (no scene loaded)\n";
    }
    else
    {
        bool any = false;
        scene->ForEach<CHEngine::TagComponent>(
            [&](CHEngine::EntityHandle h, const CHEngine::UUID&, CHEngine::TagComponent& tag)
            {
                any = true;
                ss << "  - \"" << scene->GetString(tag.Name) << "\"";

                auto* e = scene->TryGetEntity(h);
                if (e && e->HasComponent<CHEngine::TransformComponent>())
                {
                    const auto& t = e->GetComponent<CHEngine::TransformComponent>().ObjectTransform;
                    ss << "  pos(" << t.Position.x << "," << t.Position.y << "," << t.Position.z << ")";
                    if (t.Rotation.x != 0 || t.Rotation.y != 0 || t.Rotation.z != 0)
                        ss << " rot(" << t.Rotation.x << "," << t.Rotation.y << "," << t.Rotation.z << ")";
                    if (t.Scale.x != 1 || t.Scale.y != 1 || t.Scale.z != 1)
                        ss << " scale(" << t.Scale.x << "," << t.Scale.y << "," << t.Scale.z << ")";
                }
                if (e && e->HasComponent<CHEngine::LightComponent>())
                {
                    const auto& l = e->GetComponent<CHEngine::LightComponent>().LightData;
                    const char* lt = (l.Type == CHEngine::LightType::Point) ? "point_light" :
                                     (l.Type == CHEngine::LightType::Spot)  ? "spot_light"  : "dir_light";
                    ss << " [" << lt << " intensity=" << l.Intensity << "]";
                }
                if (e && e->HasComponent<CHEngine::ScriptComponent>())
                {
                    const auto& sc = e->GetComponent<CHEngine::ScriptComponent>();
                    const auto* svec = scene->GetScripts(sc.Scripts);
                    if (svec && !svec->empty())
                        ss << " [has_script:" << svec->size() << "]";
                }
                if (e && e->HasComponent<CHEngine::CameraComponent>())
                    ss << " [camera]";
                ss << "\n";
            });
        if (!any)
            ss << "  (scene is empty)\n";
    }

    // Editor camera info
    const auto& state = EditorCameraState;
    ss << "\nEDITOR CAMERA:\n";
    ss << "  Looking at: (" << state.OrbitTarget.x << "," << state.OrbitTarget.y << "," << state.OrbitTarget.z << ")";
    ss << "  Distance: " << state.OrbitDist << "\n";

    // Coordinate system hint
    ss << "\nCOORDINATES:\n";
    ss << "  X+= right, X-= left, Y+= up, Y-= down, Z+= toward viewer, Z-= away\n";
    ss << "  Objects at Z > 6 may be behind editor camera (invisible)\n";
    ss << "  Safe visible range: X[-10..10], Y[-5..10], Z[-10..5]\n";
    ss << "  'A bit to the left' = X -2..3  |  'above' = Y +2..3  |  'behind X' = same pos but Z+2\n";

    return ss.str();
}
