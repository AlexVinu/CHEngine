#include "SceneViewLayerHost.h"

#include "SceneViewLayer.h"
#include "SceneViewLayer_IO.h"
#include "SceneViewLayerAccess.h"
#include "SceneViewLayer_CameraOps.h"
#include "SceneViewLayer_IO.h"
#include "SceneViewLayer_Play.h"
#include "SceneBrowserPanel.h"

#include <CHEngine/Application.h>
#include <CHEngine/EngineConfig.h>
#include <CHEngine/Mesh/Material.h>
#include <CHEngine/Mesh/PrimitiveMeshFactory.h>
#include <CHEngine/Project/Project.h>
#include <CHEngine/Project/ProjectManager.h>
#include <CHEngine/Render/RenderFacade.h>
#include <CHEngine/ResourceManager/ResourceManager.h>
#include <CHEngine/Scene/Components.h>

#include <FileSystem/FileSystem.h>

#include "UIThemeActive.h"

#include <boost/uuid/random_generator.hpp>
#include <filesystem>
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

bool& SceneViewLayerHost::GetShowUVEditor()
{
    return SceneViewLayerAccess::ShowUVEditor(m_Layer);
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

void SceneViewLayerHost::CloseSceneSession(size_t session_index)
{
    auto& sessions = SceneViewLayerAccess::Sessions(m_Layer);
    if (sessions.size() <= 1)
        return; // Always keep at least one session open.
    if (session_index >= sessions.size())
        return;

    size_t active = SceneViewLayerAccess::ActiveIndex(m_Layer);
    sessions.erase(sessions.begin() + static_cast<std::ptrdiff_t>(session_index));

    if (active == session_index)
    {
        const size_t newIdx = (session_index > 0) ? session_index - 1 : 0;
        SceneViewLayerAccess::SetActiveIndex(m_Layer, newIdx);
    }
    else if (active > session_index)
    {
        SceneViewLayerAccess::SetActiveIndex(m_Layer, active - 1);
    }
}

void SceneViewLayerHost::OpenSceneFile(const std::string& relOrAbsPath)
{
    if (relOrAbsPath.empty())
        return;

    namespace fs = std::filesystem;
    std::string absPath = relOrAbsPath;
    std::string rel = relOrAbsPath;

    if (CHEngine::ProjectManager::HasProject())
    {
        CHEngine::Project* proj = CHEngine::ProjectManager::Current();
        if (fs::path(relOrAbsPath).is_absolute())
        {
            rel = fs::path(proj->ToRelativePath(relOrAbsPath)).generic_string();
        }
        else
        {
            absPath = (proj->RootDir() / relOrAbsPath).string();
        }
    }

    // If a session already shows this scene, just focus it.
    auto& sessions = SceneViewLayerAccess::Sessions(m_Layer);
    for (size_t i = 0; i < sessions.size(); ++i)
    {
        if (sessions[i].SceneRelPath == rel)
        {
            SceneViewLayerAccess::SetActiveIndex(m_Layer, i);
            return;
        }
    }

    // If the active session is empty/untitled, reuse it; otherwise open a new tab.
    EditorWorldContext& cur = SceneViewLayerAccess::Active(m_Layer);
    if (!cur.SceneRelPath.empty())
        AddSceneSession();

    SceneViewLayerIO::LoadSceneSilent(m_Layer, absPath);
}

void SceneViewLayerHost::ToggleSceneBrowser()
{
    SceneViewLayerAccess::SceneBrowser(m_Layer).Toggle();
}

void SceneViewLayerHost::NewSceneFile()
{
    if (!CHEngine::ProjectManager::HasProject())
        return;
    namespace fs = std::filesystem;
    CHEngine::Project* proj = CHEngine::ProjectManager::Current();
    const fs::path scenesDir = proj->ScenesAbsPath();
    std::error_code ec;
    fs::create_directories(scenesDir, ec);

    fs::path target;
    for (int i = 1; i < 10000; ++i)
    {
        target = scenesDir / ("Untitled_" + std::to_string(i) + ".chscene");
        if (!fs::exists(target))
            break;
    }
    CHEngine::FileSystem::WriteFileText(target, R"({"version":3,"objects":[]})");

    const std::string rel = fs::relative(target, proj->RootDir(), ec).generic_string();
    OpenSceneFile(rel);
}

void SceneViewLayerHost::DeleteSceneFile(const std::string& rel)
{
    if (!CHEngine::ProjectManager::HasProject() || rel.empty())
        return;
    namespace fs = std::filesystem;
    CHEngine::Project* proj = CHEngine::ProjectManager::Current();
    const fs::path abs = proj->RootDir() / rel;

    std::error_code ec;
    fs::remove(abs, ec);
    if (ec)
    {
        CHE_CORE_ERROR("DeleteSceneFile: failed to remove '{}': {}", abs.string(), ec.message());
        return;
    }

    // Close any tabs bound to it (but keep at least one session alive).
    auto& sessions = SceneViewLayerAccess::Sessions(m_Layer);
    for (size_t i = sessions.size(); i-- > 0;)
    {
        if (sessions[i].SceneRelPath == rel)
        {
            if (sessions.size() > 1)
                CloseSceneSession(i);
            else
                sessions[i].SceneRelPath.clear(); // becomes a clean Untitled tab
        }
    }

    // If we deleted the startup scene, clear that field.
    if (proj->GetStartupScene() == rel)
    {
        proj->SetStartupScene("");
        proj->Save();
    }
}

void SceneViewLayerHost::RenameSceneFile(const std::string& oldRel, const std::string& newName)
{
    if (!CHEngine::ProjectManager::HasProject() || oldRel.empty() || newName.empty())
        return;
    namespace fs = std::filesystem;
    CHEngine::Project* proj = CHEngine::ProjectManager::Current();

    fs::path oldAbs = proj->RootDir() / oldRel;
    fs::path parent = oldAbs.parent_path();

    // Strip any extension the user might have typed; we always store as .chscene.
    fs::path stem = fs::path(newName).stem();
    if (stem.empty()) stem = newName;
    fs::path newAbs = parent / (stem.string() + ".chscene");
    if (newAbs == oldAbs)
        return;

    std::error_code ec;
    if (fs::exists(newAbs))
    {
        CHE_CORE_WARN("RenameSceneFile: target already exists: {}", newAbs.string());
        return;
    }
    fs::rename(oldAbs, newAbs, ec);
    if (ec)
    {
        CHE_CORE_ERROR("RenameSceneFile: failed: {}", ec.message());
        return;
    }

    const std::string newRel = fs::relative(newAbs, proj->RootDir(), ec).generic_string();

    // Update any open tabs.
    auto& sessions = SceneViewLayerAccess::Sessions(m_Layer);
    for (auto& s : sessions)
    {
        if (s.SceneRelPath == oldRel)
            s.SceneRelPath = newRel;
    }

    // Update startup scene reference if needed.
    if (proj->GetStartupScene() == oldRel)
    {
        proj->SetStartupScene(newRel);
        proj->Save();
    }
}

void SceneViewLayerHost::SetStartupSceneFile(const std::string& rel)
{
    if (!CHEngine::ProjectManager::HasProject() || rel.empty())
        return;
    CHEngine::Project* proj = CHEngine::ProjectManager::Current();
    proj->SetStartupScene(rel);
    proj->Save();

    // Move the matching open session (if any) to index 0. Otherwise just record the pref.
    auto& sessions = SceneViewLayerAccess::Sessions(m_Layer);
    for (size_t i = 0; i < sessions.size(); ++i)
    {
        if (sessions[i].SceneRelPath == rel && i != 0)
        {
            std::swap(sessions[0], sessions[i]);
            // Keep active pointing at the same logical session.
            const size_t active = SceneViewLayerAccess::ActiveIndex(m_Layer);
            if (active == 0)
                SceneViewLayerAccess::SetActiveIndex(m_Layer, i);
            else if (active == i)
                SceneViewLayerAccess::SetActiveIndex(m_Layer, 0);
            break;
        }
    }
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
    // Legacy: kept for compatibility. Prefer ToggleSceneBrowser().
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

void SceneViewLayerHost::OnProjectChanged()
{
    m_Layer.OnProjectOpened();
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
            CHEngine::ResourceManager::Instance().Unload(d0);
        if (mat_ref->m_Material)
        {
            mat_ref->m_Material->DiffuseMap = CHEngine::TextureHandle{};
            mat_ref->m_Material->DiffuseMapPath.clear();
        }
        mat_ref->DiffuseMap = CHEngine::ResourceManager::Instance().Load<CHEngine::TextureHandle>(filepath);
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
            CHEngine::ResourceManager::Instance().Unload(d0);
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
            CHEngine::ResourceManager::Instance().Unload(s0);
        if (mat_ref->m_Material)
        {
            mat_ref->m_Material->SpecularMap = CHEngine::TextureHandle{};
            mat_ref->m_Material->SpecularMapPath.clear();
        }
        mat_ref->SpecularMap = CHEngine::ResourceManager::Instance().Load<CHEngine::TextureHandle>(filepath);
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
            CHEngine::ResourceManager::Instance().Unload(s0);
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
