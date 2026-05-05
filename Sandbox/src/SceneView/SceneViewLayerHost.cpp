#include "SceneViewLayerHost.h"

#include "SceneViewLayer.h"
#include "SceneViewLayerAccess.h"
#include "ScriptEditorPanel.h"
#include "SceneViewLayer_CameraOps.h"
#include "SceneViewLayer_IO.h"
#include "SceneViewLayer_Play.h"

#include <CHEngine/Application.h>
#include <CHEngine/EngineConfig.h>
#include <CHEngine/Mesh/Material.h>
#include <CHEngine/Mesh/PrimitiveMeshFactory.h>
#include <CHEngine/Render/RenderFacade.h>
#include <CHEngine/Scene/Components.h>

#include "UIThemeActive.h"

#include <boost/uuid/random_generator.hpp>
#include <glm/glm.hpp>

SceneViewLayerHost::SceneViewLayerHost(SceneViewLayer& layer)
    : m_Layer(layer)
{
}

EditorWorldContext& SceneViewLayerHost::GetActiveSceneSession()
{
    return SceneViewLayerAccess::Active(m_Layer);
}

Sandbox::CommandStack& SceneViewLayerHost::GetCommandStack()
{
    return SceneViewLayerAccess::Active(m_Layer).CommandStack;
}

Sandbox::EditorCameraController& SceneViewLayerHost::GetEditorCameraController()
{
    return SceneViewLayerAccess::CameraController(m_Layer);
}

Sandbox::EditorViewport& SceneViewLayerHost::GetEditorViewport()
{
    return SceneViewLayerAccess::Viewport(m_Layer);
}

ImGuizmo::OPERATION& SceneViewLayerHost::GetGizmoOperation()
{
    return SceneViewLayerAccess::Active(m_Layer).GizmoOperation;
}

ImGuizmo::MODE& SceneViewLayerHost::GetGizmoMode()
{
    return SceneViewLayerAccess::Active(m_Layer).GizmoMode;
}

bool& SceneViewLayerHost::GetLocalMode()
{
    return SceneViewLayerAccess::Active(m_Layer).LocalMode;
}

bool& SceneViewLayerHost::GetShowProfiler()
{
    return SceneViewLayerAccess::Active(m_Layer).ShowProfiler;
}

std::vector<EditorWorldContext>& SceneViewLayerHost::GetSceneSessions()
{
    return SceneViewLayerAccess::Sessions(m_Layer);
}

size_t SceneViewLayerHost::GetActiveSessionIndex() const
{
    return SceneViewLayerAccess::ActiveIndex(m_Layer);
}

void SceneViewLayerHost::SetActiveSessionIndex(size_t session_index)
{
    SceneViewLayerAccess::SetActiveIndex(m_Layer, session_index);
}

void SceneViewLayerHost::AddSceneSession()
{
    EditorWorldContext session;
    Sandbox::EditorViewport& viewport = SceneViewLayerAccess::Viewport(m_Layer);
    const EditorWorldContext& active = SceneViewLayerAccess::Active(m_Layer);
    if (viewport.GetViewportSize().x > 1.0f && viewport.GetViewportSize().y > 1.0f)
        session.ViewportSize = { viewport.GetViewportSize().x, viewport.GetViewportSize().y };
    else
        session.ViewportSize = active.ViewportSize;
    session.ViewportCamera->SetViewportSize(session.ViewportSize.x, session.ViewportSize.y);
    session.EditorCameraState = active.EditorCameraState;
    SceneViewLayerAccess::CameraController(m_Layer).ApplyOrbit(
        session.ViewportCamera.get(), session.EditorCameraState);
    SceneViewLayerAccess::Sessions(m_Layer).push_back(std::move(session));
    SceneViewLayerAccess::SetActiveIndex(m_Layer, SceneViewLayerAccess::Sessions(m_Layer).size() - 1);
}

CHEngine::Transform& SceneViewLayerHost::GetTransformBeforeDrag()
{
    return SceneViewLayerAccess::Active(m_Layer).TransformBeforeDrag;
}

void SceneViewLayerHost::RequestUndo()
{
    EditorWorldContext& ctx = SceneViewLayerAccess::Active(m_Layer);
    if (ctx.CommandStack.CanUndo())
        ctx.CommandStack.Undo();
}

void SceneViewLayerHost::OpenSceneDialog()
{
    SceneViewLayerIO::LoadScene(m_Layer, "");
}

void SceneViewLayerHost::SetViewportFov(float fov_degrees)
{
    SceneViewLayerAccess::Active(m_Layer).ViewportCamera->SetFOV(fov_degrees);
}

void SceneViewLayerHost::ResetViewportCamera()
{
    EditorWorldContext& ctx = SceneViewLayerAccess::Active(m_Layer);
    CHEngine::EditorCamera* viewportCamera = ctx.ViewportCamera.get();
    Sandbox::EditorCameraState& camera_state = ctx.EditorCameraState;
    camera_state.OrbitTarget = { 0.0f, 0.0f, 0.0f };
    camera_state.OrbitDist = 8.0f;
    viewportCamera->SetYaw(glm::radians(-90.0f));
    viewportCamera->SetPitch(glm::radians(-15.0f));
    viewportCamera->SetFOV(45.0f);
    SceneViewLayerCameraOps::ApplyOrbit(m_Layer);
}

void SceneViewLayerHost::AddDirectionalLight()
{
    EditorWorldContext& activeSession = SceneViewLayerAccess::Active(m_Layer);
    auto scene_ref = activeSession.EditorScene;
    if (!scene_ref)
        return;
    const CHEngine::UUID object_id = boost::uuids::random_generator()();
    const CHEngine::EntityHandle handle = scene_ref->CreateEntity("Directional Light", object_id);
    if (auto* entity = scene_ref->TryGetEntity(handle); entity)
    {
        entity->AddComponent<CHEngine::LightComponent>(
            CHEngine::LightComponent{ CHEngine::Light{ CHEngine::LightType::Directional } });
        if (entity->HasComponent<CHEngine::TransformComponent>())
        {
            entity->PatchComponent<CHEngine::TransformComponent>([](CHEngine::TransformComponent& transform_component) {
                transform_component.ObjectTransform.Rotation = { -45.0f, -30.0f, 0.0f };
            });
        }
        activeSession.SelectedEntity = handle;
    }
}

void SceneViewLayerHost::AddPointLight()
{
    EditorWorldContext& activeSession = SceneViewLayerAccess::Active(m_Layer);
    auto scene_ref = activeSession.EditorScene;
    if (!scene_ref)
        return;
    const CHEngine::UUID object_id = boost::uuids::random_generator()();
    const CHEngine::EntityHandle handle = scene_ref->CreateEntity("Point Light", object_id);
    if (auto* entity = scene_ref->TryGetEntity(handle); entity)
    {
        entity->AddComponent<CHEngine::LightComponent>(
            CHEngine::LightComponent{ CHEngine::Light{ CHEngine::LightType::Point } });
        if (entity->HasComponent<CHEngine::TransformComponent>())
        {
            entity->PatchComponent<CHEngine::TransformComponent>([](CHEngine::TransformComponent& transform_component) {
                transform_component.ObjectTransform.Position = { 0.0f, 3.0f, 0.0f };
            });
        }
        activeSession.SelectedEntity = handle;
    }
}

void SceneViewLayerHost::AddSpotLight()
{
    EditorWorldContext& activeSession = SceneViewLayerAccess::Active(m_Layer);
    auto scene_ref = activeSession.EditorScene;
    if (!scene_ref)
        return;
    const CHEngine::UUID object_id = boost::uuids::random_generator()();
    const CHEngine::EntityHandle handle = scene_ref->CreateEntity("Spot Light", object_id);
    if (auto* entity = scene_ref->TryGetEntity(handle); entity)
    {
        entity->AddComponent<CHEngine::LightComponent>(
            CHEngine::LightComponent{ CHEngine::Light{ CHEngine::LightType::Spot } });
        if (entity->HasComponent<CHEngine::TransformComponent>())
        {
            entity->PatchComponent<CHEngine::TransformComponent>([](CHEngine::TransformComponent& transform_component) {
                transform_component.ObjectTransform.Position = { 0.0f, 5.0f, 0.0f };
                transform_component.ObjectTransform.Rotation = { -90.0f, 0.0f, 0.0f };
            });
        }
        activeSession.SelectedEntity = handle;
    }
}

void SceneViewLayerHost::AddCubePrimitive()
{
    EditorWorldContext& activeSession = SceneViewLayerAccess::Active(m_Layer);
    auto scene_ref = activeSession.EditorScene;
    if (!scene_ref)
        return;

    const CHEngine::UUID object_id = boost::uuids::random_generator()();
    const CHEngine::EntityHandle handle = scene_ref->CreateEntity("Cube", object_id);
    auto* entity = scene_ref->TryGetEntity(handle);
    if (!entity)
        return;

    if (!entity->HasComponent<CHEngine::MeshComponent>())
        entity->AddComponent<CHEngine::MeshComponent>(CHEngine::MeshComponent{});

    CHEngine::Mesh cube_mesh = CHEngine::PrimitiveMeshFactory::CreateCube(1.0f, { 0.8f, 0.8f, 0.8f });
    cube_mesh.Mat = CHEngine::MaterialInstance::FromBase(
        std::make_shared<CHEngine::Material>(SceneViewLayerAccess::Viewport(m_Layer).GetMeshShader()));
    entity->PatchComponent<CHEngine::MeshComponent>(
        [cube_mesh = std::move(cube_mesh)](CHEngine::MeshComponent& mesh_component) mutable {
            mesh_component.Meshes.clear();
            mesh_component.Meshes.push_back(std::move(cube_mesh));
            mesh_component.SourcePath = ":primitive:cube";
        });

    activeSession.SelectedEntity = handle;
}

void SceneViewLayerHost::AddEmptyEntity()
{
    EditorWorldContext& activeSession = SceneViewLayerAccess::Active(m_Layer);
    auto scene_ref = activeSession.EditorScene;
    if (!scene_ref)
        return;
    const CHEngine::UUID object_id = boost::uuids::random_generator()();
    const CHEngine::EntityHandle handle = scene_ref->CreateEntity("New Object", object_id);
    activeSession.SelectedEntity = handle;
}

void SceneViewLayerHost::SetSelection(CHEngine::EntityHandle handle)
{
    SceneViewLayerAccess::Active(m_Layer).SelectedEntity = handle;
}

void SceneViewLayerHost::DestroyEntityByUuid(const CHEngine::UUID& object_id)
{
    EditorWorldContext& activeSession = SceneViewLayerAccess::Active(m_Layer);
    auto scene_ref = activeSession.EditorScene;
    if (!scene_ref)
        return;
    if (scene_ref->IsEntityHandleValid(activeSession.SelectedEntity)
        && scene_ref->GetUUID(activeSession.SelectedEntity) == object_id)
        activeSession.SelectedEntity = {};
    scene_ref->DestroyEntity(object_id);
}

void SceneViewLayerHost::OnRendererApiSelected(CHEngine::ERenderAPI api)
{
    CHEngine::EngineConfig::SaveRendererPreference(api);
    AutoSaveForRestart();
    CHEngine::Application::Get().RequestRestart();
}

void SceneViewLayerHost::ToggleUiTheme()
{
    AppTheme next = (UIActive::g_Theme == AppTheme::RetroOS) ? AppTheme::Dark : AppTheme::RetroOS;
    UIActive::SetTheme(next);
}

void SceneViewLayerHost::ApplyDiffuseTextureToSelectedSubmesh(size_t submesh_index, const std::string& filepath)
{
    EditorWorldContext& activeSession = SceneViewLayerAccess::Active(m_Layer);
    CHEngine::Scene* scene = activeSession.EditorScene.get();
    const CHEngine::EntityHandle selectedHandle = activeSession.SelectedEntity;
    if (!scene || !scene->IsEntityHandleValid(selectedHandle))
        return;
    auto* selectedEntity = scene->TryGetEntity(selectedHandle);
    if (!selectedEntity || !selectedEntity->HasComponent<CHEngine::MeshComponent>())
        return;
    const auto& meshComp = selectedEntity->GetComponent<CHEngine::MeshComponent>();
    if (submesh_index >= meshComp.Meshes.size())
        return;
    selectedEntity->PatchComponent<CHEngine::MeshComponent>([&](CHEngine::MeshComponent& mesh_component) {
        auto& subMesh = mesh_component.Meshes[submesh_index];
        if (!subMesh.Mat)
            subMesh.Mat = CHEngine::MaterialInstance::FromBase(std::make_shared<CHEngine::Material>(
                SceneViewLayerAccess::Viewport(m_Layer).GetMeshShader()));
        auto mat_ref = subMesh.Mat;
        CHEngine::TextureHandle d0, s0;
        mat_ref->ResolveTextures(d0, s0);
        if (d0.IsValid())
            CHEngine::RenderFacade::DestroyTexture(d0);
        if (mat_ref->m_Material)
        {
            mat_ref->m_Material->DiffuseMap = CHEngine::TextureHandle{};
            mat_ref->m_Material->DiffuseMapPath.clear();
        }
        mat_ref->DiffuseMap = CHEngine::RenderFacade::CreateTextureFromFile(filepath);
        mat_ref->DiffuseMapPath = mat_ref->DiffuseMap.IsValid() ? filepath : "";
    });
}

void SceneViewLayerHost::ClearDiffuseTextureOnSelectedSubmesh(size_t submesh_index)
{
    EditorWorldContext& activeSession = SceneViewLayerAccess::Active(m_Layer);
    CHEngine::Scene* scene = activeSession.EditorScene.get();
    const CHEngine::EntityHandle selectedHandle = activeSession.SelectedEntity;
    if (!scene || !scene->IsEntityHandleValid(selectedHandle))
        return;
    auto* selectedEntity = scene->TryGetEntity(selectedHandle);
    if (!selectedEntity || !selectedEntity->HasComponent<CHEngine::MeshComponent>())
        return;
    const auto& meshComp = selectedEntity->GetComponent<CHEngine::MeshComponent>();
    if (submesh_index >= meshComp.Meshes.size())
        return;
    selectedEntity->PatchComponent<CHEngine::MeshComponent>([&](CHEngine::MeshComponent& mesh_component) {
        auto& subMesh = mesh_component.Meshes[submesh_index];
        if (!subMesh.Mat)
            return;
        auto mat_ref = subMesh.Mat;
        CHEngine::TextureHandle d0, s0;
        mat_ref->ResolveTextures(d0, s0);
        if (d0.IsValid())
            CHEngine::RenderFacade::DestroyTexture(d0);
        mat_ref->DiffuseMap = CHEngine::TextureHandle{};
        mat_ref->DiffuseMapPath.clear();
        if (mat_ref->m_Material)
        {
            mat_ref->m_Material->DiffuseMap = CHEngine::TextureHandle{};
            mat_ref->m_Material->DiffuseMapPath.clear();
        }
    });
}

void SceneViewLayerHost::ApplySpecularTextureToSelectedSubmesh(size_t submesh_index, const std::string& filepath)
{
    EditorWorldContext& activeSession = SceneViewLayerAccess::Active(m_Layer);
    CHEngine::Scene* scene = activeSession.EditorScene.get();
    const CHEngine::EntityHandle selectedHandle = activeSession.SelectedEntity;
    if (!scene || !scene->IsEntityHandleValid(selectedHandle))
        return;
    auto* selectedEntity = scene->TryGetEntity(selectedHandle);
    if (!selectedEntity || !selectedEntity->HasComponent<CHEngine::MeshComponent>())
        return;
    const auto& meshComp = selectedEntity->GetComponent<CHEngine::MeshComponent>();
    if (submesh_index >= meshComp.Meshes.size())
        return;
    selectedEntity->PatchComponent<CHEngine::MeshComponent>([&](CHEngine::MeshComponent& mesh_component) {
        auto& subMesh = mesh_component.Meshes[submesh_index];
        if (!subMesh.Mat)
            subMesh.Mat = CHEngine::MaterialInstance::FromBase(std::make_shared<CHEngine::Material>(
                SceneViewLayerAccess::Viewport(m_Layer).GetMeshShader()));
        auto mat_ref = subMesh.Mat;
        CHEngine::TextureHandle d0, s0;
        mat_ref->ResolveTextures(d0, s0);
        if (s0.IsValid())
            CHEngine::RenderFacade::DestroyTexture(s0);
        if (mat_ref->m_Material)
        {
            mat_ref->m_Material->SpecularMap = CHEngine::TextureHandle{};
            mat_ref->m_Material->SpecularMapPath.clear();
        }
        mat_ref->SpecularMap = CHEngine::RenderFacade::CreateTextureFromFile(filepath);
        mat_ref->SpecularMapPath = mat_ref->SpecularMap.IsValid() ? filepath : "";
    });
}

void SceneViewLayerHost::ClearSpecularTextureOnSelectedSubmesh(size_t submesh_index)
{
    EditorWorldContext& activeSession = SceneViewLayerAccess::Active(m_Layer);
    CHEngine::Scene* scene = activeSession.EditorScene.get();
    const CHEngine::EntityHandle selectedHandle = activeSession.SelectedEntity;
    if (!scene || !scene->IsEntityHandleValid(selectedHandle))
        return;
    auto* selectedEntity = scene->TryGetEntity(selectedHandle);
    if (!selectedEntity || !selectedEntity->HasComponent<CHEngine::MeshComponent>())
        return;
    const auto& meshComp = selectedEntity->GetComponent<CHEngine::MeshComponent>();
    if (submesh_index >= meshComp.Meshes.size())
        return;
    selectedEntity->PatchComponent<CHEngine::MeshComponent>([&](CHEngine::MeshComponent& mesh_component) {
        auto& subMesh = mesh_component.Meshes[submesh_index];
        if (!subMesh.Mat)
            return;
        auto mat_ref = subMesh.Mat;
        CHEngine::TextureHandle d0, s0;
        mat_ref->ResolveTextures(d0, s0);
        if (s0.IsValid())
            CHEngine::RenderFacade::DestroyTexture(s0);
        mat_ref->SpecularMap = CHEngine::TextureHandle{};
        mat_ref->SpecularMapPath.clear();
        if (mat_ref->m_Material)
        {
            mat_ref->m_Material->SpecularMap = CHEngine::TextureHandle{};
            mat_ref->m_Material->SpecularMapPath.clear();
        }
    });
}

void SceneViewLayerHost::SaveScene()
{
    SceneViewLayerIO::SaveScene(m_Layer);
}

void SceneViewLayerHost::AutoSaveForRestart()
{
    SceneViewLayerIO::AutoSaveForRestart(m_Layer);
}

void SceneViewLayerHost::ImportModel(const std::string& filepath)
{
    SceneViewLayerIO::ImportModel(m_Layer, filepath);
}

void SceneViewLayerHost::ApplyOrbit()
{
    SceneViewLayerCameraOps::ApplyOrbit(m_Layer);
}

void SceneViewLayerHost::SetViewPreset(float yaw_degrees, float pitch_degrees)
{
    SceneViewLayerCameraOps::SetViewPreset(m_Layer, yaw_degrees, pitch_degrees);
}

void SceneViewLayerHost::FocusOnSelected()
{
    SceneViewLayerCameraOps::FocusOnSelected(m_Layer);
}

void SceneViewLayerHost::EnterPlayMode()
{
    SceneViewLayerPlay::EnterPlayMode(m_Layer);
}

void SceneViewLayerHost::EnterPauseMode()
{
    SceneViewLayerPlay::EnterPauseMode(m_Layer);
}

void SceneViewLayerHost::ResumeFromPause()
{
    SceneViewLayerPlay::ResumeFromPause(m_Layer);
}

void SceneViewLayerHost::StopPlayMode()
{
    SceneViewLayerPlay::StopPlayMode(m_Layer);
}

void SceneViewLayerHost::OpenScriptEditor(const std::string& scriptPath)
{
    SceneViewLayerAccess::ScriptEditor(m_Layer).Open(scriptPath);
}

void SceneViewLayerHost::NewScriptEditor(const std::string& scriptPath)
{
    SceneViewLayerAccess::ScriptEditor(m_Layer).NewScript(scriptPath);
}
