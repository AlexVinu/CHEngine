#include "SceneViewLayerHost.h"

#include "SceneViewLayer.h"
#include "SceneViewLayer_IO.h"
#include "SceneViewLayerAccess.h"
#include "SceneViewLayer_CameraOps.h"
#include "SceneViewLayer_IO.h"
#include "SceneViewLayer_Play.h"
#include "SceneBrowserPanel.h"
#include "ProjectManager.h"

#include <CHEngine/Application.h>
#include <CHEngine/EngineConfig.h>
#include <CHEngine/Mesh/Material.h>
#include <CHEngine/ResourceManager/ResourceManager.h>
#include <CHEngine/Scene/Components.h>
#include <CHEngine/Camera/PerspectiveCamera.h>
#include <glm/gtc/constants.hpp>

#include <FileSystem/FileSystem.h>

#include "UIThemeActive.h"

#include <filesystem>
#include <sstream>
#include <iomanip>
#include <glm/glm.hpp>

namespace Sandbox {

SceneViewLayerHost::SceneViewLayerHost(SceneViewLayer& layer)
    : m_Layer(layer)
{
}

EditorWorldContext* SceneViewLayerHost::GetActiveSceneSession()
{
    return SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
}

Ref<ProjectManager> SceneViewLayerHost::GetProjectManager()
{
    return SceneViewLayerAccess::ProjectManagerRef(m_Layer);
}

Sandbox::CommandStack& SceneViewLayerHost::GetCommandStack()
{
    return SceneViewLayerAccess::ActiveWorldCtx(m_Layer)->CommandStack;
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
    return SceneViewLayerAccess::ActiveWorldCtx(m_Layer)->GizmoOperation;
}

ImGuizmo::MODE& SceneViewLayerHost::GetGizmoMode()
{
    return SceneViewLayerAccess::ActiveWorldCtx(m_Layer)->GizmoMode;
}

bool& SceneViewLayerHost::GetLocalMode()
{
    return SceneViewLayerAccess::ActiveWorldCtx(m_Layer)->LocalMode;
}

bool& SceneViewLayerHost::GetShowProfiler()
{
    return SceneViewLayerAccess::ActiveWorldCtx(m_Layer)->ShowProfiler;
}

size_t SceneViewLayerHost::GetActiveSessionIndex() const
{
    return SceneViewLayerAccess::ActiveIndex(m_Layer);
}

void SceneViewLayerHost::SetActiveSessionIndex(size_t session_index)
{
    SceneViewLayerAccess::SetActiveIndex(m_Layer, session_index);
}

std::vector<EditorWorldContext*> SceneViewLayerHost::GetSceneSessions() const
{
    auto* wl = SceneViewLayerAccess::WorldsList(m_Layer);
    std::vector<EditorWorldContext*> result;
    result.reserve(wl->Size());
    wl->ForEach([&](CHEngine::World& w) {
        result.push_back(static_cast<EditorWorldContext*>(&w));
    });
    return result;
}

void SceneViewLayerHost::AddSceneSession()
{
    auto world_list = SceneViewLayerAccess::WorldsList(m_Layer);
    auto ctx = new EditorWorldContext(world_list);
    world_list->PushBack(ctx);

    Sandbox::EditorViewport& viewport = SceneViewLayerAccess::Viewport(m_Layer);
    const auto active = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    if (viewport.GetViewportSize().x > 1.0f && viewport.GetViewportSize().y > 1.0f)
        ctx->ViewportSize = { viewport.GetViewportSize().x, viewport.GetViewportSize().y };
    else
        ctx->ViewportSize = active->ViewportSize;
    ctx->ViewportCamera->SetViewportSize(ctx->ViewportSize.x, ctx->ViewportSize.y);
    ctx->EditorCameraState = active->EditorCameraState;
    SceneViewLayerAccess::CameraController(m_Layer).ApplyOrbit(
        ctx->ViewportCamera.get(), ctx->EditorCameraState);
    SceneViewLayerAccess::SetActiveIndex(m_Layer, world_list->Size() - 1);
}

void SceneViewLayerHost::CloseSceneSession(size_t session_index)
{
    auto world_list = SceneViewLayerAccess::WorldsList(m_Layer);
    if (world_list->Size() <= 1)
        return; // Always keep at least one session open.
    if (session_index >= world_list->Size())
        return;

    size_t active = SceneViewLayerAccess::ActiveIndex(m_Layer);
    world_list->Erase(session_index);

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

    Ref<ProjectManager> proj_manager = SceneViewLayerAccess::ProjectManagerRef(m_Layer);

    if (proj_manager->HasProject())
    {
        Project* proj = proj_manager->Current();
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
    auto world_list = SceneViewLayerAccess::WorldsList(m_Layer);
    for (size_t i = 0; i < world_list->Size(); ++i)
    {
        auto session = static_cast<EditorWorldContext*>(world_list->GetForIndex(i));
        if (session->SceneRelPath == rel)
        {
            SceneViewLayerAccess::SetActiveIndex(m_Layer, i);
            return;
        }
    }

    // If the active session is empty/untitled, reuse it; otherwise open a new tab.
    auto cur = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    if (!cur->SceneRelPath.empty())
        AddSceneSession();

    SceneViewLayerIO::LoadSceneSilent(m_Layer, absPath);
}

void SceneViewLayerHost::ToggleSceneBrowser()
{
    SceneViewLayerAccess::SceneBrowser(m_Layer).Toggle();
}

void SceneViewLayerHost::NewSceneFile()
{
    Ref<ProjectManager> proj_manager = SceneViewLayerAccess::ProjectManagerRef(m_Layer);
    if (!proj_manager->HasProject())
        return;
    const std::string rel = proj_manager->Current()->CreateScene();
    if (!rel.empty())
        OpenSceneFile(rel);
}

void SceneViewLayerHost::DeleteSceneFile(const std::string& rel)
{
    Ref<ProjectManager> proj_manager = SceneViewLayerAccess::ProjectManagerRef(m_Layer);
    if (!proj_manager->HasProject() || rel.empty())
        return;

    if (!proj_manager->Current()->DeleteScene(rel))
        return;

    // Close any tabs bound to it (but keep at least one session alive).
    auto* wl = SceneViewLayerAccess::WorldsList(m_Layer);
    for (size_t i = wl->Size(); i-- > 0;)
    {
        auto* s = static_cast<EditorWorldContext*>(wl->GetForIndex(i));
        if (s->SceneRelPath == rel)
        {
            if (wl->Size() > 1)
                CloseSceneSession(i);
            else
                s->SceneRelPath.clear();
        }
    }
}

void SceneViewLayerHost::RenameSceneFile(const std::string& oldRel, const std::string& newName)
{
    Ref<ProjectManager> proj_manager = SceneViewLayerAccess::ProjectManagerRef(m_Layer);
    if (!proj_manager->HasProject() || oldRel.empty() || newName.empty())
        return;

    const std::string newRel = proj_manager->Current()->RenameScene(oldRel, newName);
    if (newRel.empty())
        return;

    // Update any open tabs.
    auto* wl = SceneViewLayerAccess::WorldsList(m_Layer);
    wl->ForEach([&](CHEngine::World& w) {
        auto& s = static_cast<EditorWorldContext&>(w);
        if (s.SceneRelPath == oldRel)
            s.SceneRelPath = newRel;
    });
}

void SceneViewLayerHost::SetStartupSceneFile(const std::string& rel)
{
    Ref<ProjectManager> proj_manager = SceneViewLayerAccess::ProjectManagerRef(m_Layer);
    if (!proj_manager->HasProject() || rel.empty())
        return;
    Project* proj = proj_manager->Current();
    proj->SetStartupScene(rel);
    proj->Save();

    // Move the matching open session (if any) to index 0. Otherwise just record the pref.
    auto* wl = SceneViewLayerAccess::WorldsList(m_Layer);
    for (size_t i = 0; i < wl->Size(); ++i)
    {
        auto* s = static_cast<EditorWorldContext*>(wl->GetForIndex(i));
        if (s->SceneRelPath == rel && i != 0)
        {
            wl->Swap(0, i);
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
    return SceneViewLayerAccess::ActiveWorldCtx(m_Layer)->TransformBeforeDrag;
}

void SceneViewLayerHost::RequestUndo()
{
    auto ctx = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    if (ctx->CommandStack.CanUndo())
        ctx->CommandStack.Undo();
}

void SceneViewLayerHost::OpenSceneDialog()
{
    // Legacy: kept for compatibility. Prefer ToggleSceneBrowser().
    SceneViewLayerIO::LoadScene(m_Layer, "");
}

void SceneViewLayerHost::ResetViewportCamera()
{
    auto ctx = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    CHEngine::EditorCamera* viewportCamera = ctx->ViewportCamera.get();
    Sandbox::EditorCameraState& camera_state = ctx->EditorCameraState;
    camera_state.OrbitTarget = { 0.0f, 0.0f, 0.0f };
    camera_state.OrbitDist = 8.0f;
    viewportCamera->SetYaw(glm::radians(-90.0f));
    viewportCamera->SetPitch(glm::radians(-15.0f));
    SceneViewLayerCameraOps::ApplyOrbit(m_Layer);
}

void SceneViewLayerHost::AddDirectionalLight()
{
    auto activeSession = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    auto scene_ref = activeSession->EditorScene;
    if (!scene_ref)
        return;
    const CHEngine::UUID object_id = CHEngine::UUID::Generate();
    const CHEngine::EntityHandle handle = scene_ref->CreateEntity("Directional Light", object_id);
    if (auto* entity = scene_ref->TryGetEntity(handle); entity)
    {
        entity->AddComponent<CHEngine::TransformComponent>();
        entity->AddComponent<CHEngine::LightComponent>(
            CHEngine::LightComponent{ CHEngine::Light{ CHEngine::LightType::Directional } });
        entity->PatchComponent<CHEngine::TransformComponent>([](CHEngine::TransformComponent& transform_component) {
            transform_component.ObjectTransform.Rotation = { -45.0f, -30.0f, 0.0f };
        });
        activeSession->SelectedEntity = handle;
    }
}

void SceneViewLayerHost::AddPointLight()
{
    auto activeSession = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    auto scene_ref = activeSession->EditorScene;
    if (!scene_ref)
        return;
    const CHEngine::UUID object_id = CHEngine::UUID::Generate();
    const CHEngine::EntityHandle handle = scene_ref->CreateEntity("Point Light", object_id);
    if (auto* entity = scene_ref->TryGetEntity(handle); entity)
    {
        entity->AddComponent<CHEngine::TransformComponent>();
        entity->AddComponent<CHEngine::LightComponent>(
            CHEngine::LightComponent{ CHEngine::Light{ CHEngine::LightType::Point } });
        entity->PatchComponent<CHEngine::TransformComponent>([](CHEngine::TransformComponent& transform_component) {
            transform_component.ObjectTransform.Position = { 0.0f, 3.0f, 0.0f };
        });
        activeSession->SelectedEntity = handle;
    }
}

void SceneViewLayerHost::AddSpotLight()
{
    auto activeSession = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    auto scene_ref = activeSession->EditorScene;
    if (!scene_ref)
        return;
    const CHEngine::UUID object_id = CHEngine::UUID::Generate();
    const CHEngine::EntityHandle handle = scene_ref->CreateEntity("Spot Light", object_id);
    if (auto* entity = scene_ref->TryGetEntity(handle); entity)
    {
        entity->AddComponent<CHEngine::TransformComponent>();
        entity->AddComponent<CHEngine::LightComponent>(
            CHEngine::LightComponent{ CHEngine::Light{ CHEngine::LightType::Spot } });
        entity->PatchComponent<CHEngine::TransformComponent>([](CHEngine::TransformComponent& transform_component) {
            transform_component.ObjectTransform.Position = { 0.0f, 5.0f, 0.0f };
            transform_component.ObjectTransform.Rotation = { -90.0f, 0.0f, 0.0f };
        });
        activeSession->SelectedEntity = handle;
    }
}

void SceneViewLayerHost::AddCubePrimitive()
{
    auto activeSession = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    auto scene_ref = activeSession->EditorScene;
    if (!scene_ref)
        return;

    const CHEngine::UUID object_id = CHEngine::UUID::Generate();
    const CHEngine::EntityHandle handle = scene_ref->CreateEntity("Cube", object_id);
    auto* entity = scene_ref->TryGetEntity(handle);
    if (!entity)
        return;

    entity->AddComponent<CHEngine::TransformComponent>();
    entity->AddComponent<CHEngine::MeshComponent>();

    CHEngine::MeshRef cube_mesh = CHEngine::Application::Get().Resources().LoadPrimitiveMesh(":primitive:cube");
    CHEngine::Application::Get().Resources().GetMeshLoader()->SetMaterial(cube_mesh.Handle(), 0,
        CHEngine::MaterialInstance::FromBase(
            std::make_shared<CHEngine::Material>(SceneViewLayerAccess::Viewport(m_Layer).GetMeshShader())));
    entity->PatchComponent<CHEngine::MeshComponent>(
        [cube_mesh = std::move(cube_mesh)](CHEngine::MeshComponent& mesh_component) mutable {
            mesh_component.Mesh = std::move(cube_mesh);
            mesh_component.SourcePath = ":primitive:cube";
        });

    activeSession->SelectedEntity = handle;
}

void SceneViewLayerHost::AddSpherePrimitive()
{
    auto activeSession = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    auto scene_ref = activeSession->EditorScene;
    if (!scene_ref)
        return;

    const CHEngine::UUID object_id = CHEngine::UUID::Generate();
    const CHEngine::EntityHandle handle = scene_ref->CreateEntity("Sphere", object_id);
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
            std::make_shared<CHEngine::Material>(SceneViewLayerAccess::Viewport(m_Layer).GetSphereImpostorShader())));
    entity->PatchComponent<CHEngine::MeshComponent>(
        [sphere_mesh = std::move(sphere_mesh)](CHEngine::MeshComponent& mesh_component) mutable {
            mesh_component.Mesh = std::move(sphere_mesh);
            mesh_component.SourcePath = ":primitive:sphere";
        });

    activeSession->SelectedEntity = handle;
}

void SceneViewLayerHost::AddCameraEntity()
{
    auto activeSession = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    auto scene_ref = activeSession->EditorScene;
    if (!scene_ref)
        return;

    const CHEngine::UUID object_id = CHEngine::UUID::Generate();
    const CHEngine::EntityHandle handle = scene_ref->CreateEntity("Camera", object_id);
    auto* entity = scene_ref->TryGetEntity(handle);
    if (!entity)
        return;

    entity->AddComponent<CHEngine::TransformComponent>();
    entity->AddComponent<CHEngine::CameraComponent>();
    activeSession->SelectedEntity = handle;
}

void SceneViewLayerHost::AddEmptyEntity()
{
    auto activeSession = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    auto scene_ref = activeSession->EditorScene;
    if (!scene_ref)
        return;
    const CHEngine::UUID object_id = CHEngine::UUID::Generate();
    const CHEngine::EntityHandle handle = scene_ref->CreateEntity("New Object", object_id);
    if (auto* entity = scene_ref->TryGetEntity(handle); entity)
        entity->AddComponent<CHEngine::TransformComponent>();
    activeSession->SelectedEntity = handle;
}

void SceneViewLayerHost::SetSelection(CHEngine::EntityHandle handle)
{
    SceneViewLayerAccess::ActiveWorldCtx(m_Layer)->SelectedEntity = handle;
}

void SceneViewLayerHost::AddUIOverlayCanvas()
{
    auto session = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    auto scene   = session->EditorScene;
    if (!scene) return;
    auto h = scene->CreateEntity("UI Overlay Canvas", CHEngine::UUID::Generate());
    auto* e = scene->TryGetEntity(h);
    if (!e) return;
    e->AddComponent<CHEngine::UIOverlayCanvasComponent>();
    e->AddComponent<CHEngine::ColorComponent>(CHEngine::ColorComponent{ { 0.08f, 0.08f, 0.10f, 0.6f } });
    session->SelectedEntity = h;
}

void SceneViewLayerHost::AddUIWorldCanvas()
{
    auto session = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    auto scene   = session->EditorScene;
    if (!scene) return;
    auto h = scene->CreateEntity("UI World Canvas", CHEngine::UUID::Generate());
    auto* e = scene->TryGetEntity(h);
    if (!e) return;
    e->AddComponent<CHEngine::TransformComponent>();
    e->AddComponent<CHEngine::UIWorldCanvasComponent>();
    e->AddComponent<CHEngine::ColorComponent>(CHEngine::ColorComponent{ { 0.08f, 0.08f, 0.10f, 0.6f } });
    session->SelectedEntity = h;
}

void SceneViewLayerHost::AddUIPanel(const CHEngine::UUID& uuid)
{
    auto session = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    auto scene   = session->EditorScene;
    if (!scene) return;
	auto h = scene->CreateEntity("UI Panel", CHEngine::UUID::Generate());
	auto* e = scene->TryGetEntity(h);
	if (!e) return;
	e->AddComponent<CHEngine::UIPanelComponent>();
	e->AddComponent<CHEngine::ColorComponent>(CHEngine::ColorComponent{ { 0.10f, 0.10f, 0.12f, 0.90f } });
    e->AddComponent<CHEngine::ParentNodeComponent>(uuid);
    e->AddComponent<CHEngine::UIRectTransformComponent>();
    session->SelectedEntity = h;
}

void SceneViewLayerHost::AddUIText(const CHEngine::UUID& uuid)
{
    auto session = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    auto scene   = session->EditorScene;
    if (!scene) return;
	auto h = scene->CreateEntity("UI Text", CHEngine::UUID::Generate());
	auto* e = scene->TryGetEntity(h);
	if (!e) return;
	e->AddComponent<CHEngine::UITextComponent>();
	e->AddComponent<CHEngine::ColorComponent>(CHEngine::ColorComponent{ { 0.10f, 0.10f, 0.12f, 0.90f } });
	e->AddComponent<CHEngine::ParentNodeComponent>(uuid);
	e->AddComponent<CHEngine::UIRectTransformComponent>();
	session->SelectedEntity = h;
}

void SceneViewLayerHost::AddUIButton(const CHEngine::UUID& uuid)
{
    auto session = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    auto scene   = session->EditorScene;
    if (!scene) return;
	auto h = scene->CreateEntity("UI Button", CHEngine::UUID::Generate());
	auto* e = scene->TryGetEntity(h);
	if (!e) return;
	e->AddComponent<CHEngine::UIButtonComponent>();
	e->AddComponent<CHEngine::ColorComponent>(CHEngine::ColorComponent{ { 0.10f, 0.10f, 0.12f, 0.90f } });
	e->AddComponent<CHEngine::ParentNodeComponent>(uuid);
	e->AddComponent<CHEngine::UIRectTransformComponent>();
	session->SelectedEntity = h;
}

void SceneViewLayerHost::AddUIImage(const CHEngine::UUID& uuid)
{
    auto session = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    auto scene   = session->EditorScene;
    if (!scene) return;
	auto h = scene->CreateEntity("UI Image", CHEngine::UUID::Generate());
	auto* e = scene->TryGetEntity(h);
	if (!e) return;
	e->AddComponent<CHEngine::UIImageComponent>();
	e->AddComponent<CHEngine::ColorComponent>(CHEngine::ColorComponent{ { 0.10f, 0.10f, 0.12f, 0.90f } });
	e->AddComponent<CHEngine::ParentNodeComponent>(uuid);
	e->AddComponent<CHEngine::UIRectTransformComponent>();
	session->SelectedEntity = h;
}

void SceneViewLayerHost::AddUISlider(const CHEngine::UUID& uuid)
{
    auto session = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    auto scene   = session->EditorScene;
    if (!scene) return;
	auto h = scene->CreateEntity("UI Slider", CHEngine::UUID::Generate());
	auto* e = scene->TryGetEntity(h);
	if (!e) return;
	e->AddComponent<CHEngine::UISliderComponent>();
	e->AddComponent<CHEngine::ColorComponent>(CHEngine::ColorComponent{ { 0.10f, 0.10f, 0.12f, 0.90f } });
	e->AddComponent<CHEngine::ParentNodeComponent>(uuid);
	e->AddComponent<CHEngine::UIRectTransformComponent>();
	session->SelectedEntity = h;
}

void SceneViewLayerHost::DestroyEntityByUuid(const CHEngine::UUID& object_id)
{
    EditorWorldContext* activeSession = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    auto scene_ref = activeSession->EditorScene;
    if (!scene_ref)
        return;
    if (scene_ref->IsEntityHandleValid(activeSession->SelectedEntity)
        && scene_ref->GetUUID(activeSession->SelectedEntity) == object_id)
        activeSession->SelectedEntity = {};
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

void SceneViewLayerHost::OpenScriptInEditor(const std::string& path)
{
    namespace fs = std::filesystem;
    std::string absPath = path;
    Ref<ProjectManager> pm = SceneViewLayerAccess::ProjectManagerRef(m_Layer);
    if (pm->HasProject() && !path.empty() && !fs::path(path).is_absolute())
        absPath = (pm->Current()->RootDir() / path).string();

    SceneViewLayerAccess::ScriptEditor(m_Layer).Open(absPath);

    // Auto-add Script Editor to tiling if it's not visible yet
    Sandbox::TilingManager& tiling = SceneViewLayerAccess::Tiling(m_Layer);
    if (!tiling.IsVisible(Sandbox::PanelID::ScriptEditor))
    {
        // Insert below Viewport (or fallback to ContentBrowser).
        // NB: Не использовать имя `near` — это макрос из windef.h.
        Sandbox::PanelID nearPanel = tiling.IsVisible(Sandbox::PanelID::ContentBrowser)
            ? Sandbox::PanelID::ContentBrowser
            : Sandbox::PanelID::Viewport;
        tiling.InsertPanel(Sandbox::PanelID::ScriptEditor, nearPanel, Sandbox::DropEdge::Right);
    }
}

void SceneViewLayerHost::CreateAndAttachScript(CHEngine::EntityHandle handle, const std::string& entityName)
{
    namespace fs = std::filesystem;
    Ref<ProjectManager> pm = SceneViewLayerAccess::ProjectManagerRef(m_Layer);
    if (!pm->HasProject()) return;

    Project* proj = pm->Current();
    fs::path scriptsDir = proj->ScriptsAbsPath();
    if (!fs::exists(scriptsDir))
        fs::create_directories(scriptsDir);

    // Sanitize entity name for use as filename
    std::string base = entityName.empty() ? "Entity" : entityName;
    for (auto& c : base)
        if (c == ' ' || c == '/' || c == '\\' || c == ':') c = '_';

    // Find unique path
    fs::path scriptPath = scriptsDir / (base + ".lua");
    int suffix = 1;
    while (fs::exists(scriptPath))
        scriptPath = scriptsDir / (base + "_" + std::to_string(suffix++) + ".lua");

    // Create file and open in editor
    auto& editor = SceneViewLayerAccess::ScriptEditor(m_Layer);
    editor.NewScript(scriptPath.string());

    // Auto-add Script Editor to tiling if not visible
    Sandbox::TilingManager& tiling = SceneViewLayerAccess::Tiling(m_Layer);
    if (!tiling.IsVisible(Sandbox::PanelID::ScriptEditor))
    {
        Sandbox::PanelID nearPanel = tiling.IsVisible(Sandbox::PanelID::ContentBrowser)
            ? Sandbox::PanelID::ContentBrowser
            : Sandbox::PanelID::Viewport;
        tiling.InsertPanel(Sandbox::PanelID::ScriptEditor, nearPanel, Sandbox::DropEdge::Right);
    }

    // Attach ScriptComponent — store absolute path so LuaScriptSystem can find the file.
    // NOTE: absolute paths are not portable across machines. To fix portability,
    // LuaScriptSystem should resolve paths relative to the project root.
    // TODO: store relative path, resolve to absolute in LuaScriptSystem using project root.
    auto session = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    auto scene = session->EditorScene;
    if (!scene) return;
    auto* entity = scene->TryGetEntity(handle);
    if (!entity) return;

    std::string absScriptPath = scriptPath.string();
    if (!entity->HasComponent<CHEngine::ScriptComponent>())
    {
        CHEngine::ScriptComponent sc;
        sc.Scripts.push_back(CHEngine::ScriptEntry{ absScriptPath, true });
        entity->AddComponent<CHEngine::ScriptComponent>(std::move(sc));
    }
    else
    {
        entity->PatchComponent<CHEngine::ScriptComponent>(
            [&](CHEngine::ScriptComponent& sc) {
                // Не дублируем тот же путь.
                for (const auto& e : sc.Scripts)
                    if (e.Path == absScriptPath) return;
                sc.Scripts.push_back(CHEngine::ScriptEntry{ absScriptPath, true });
            });
    }
}

void SceneViewLayerHost::ApplyLayoutPreset(const std::string& presetName)
{
    SceneViewLayerAccess::Tiling(m_Layer).ApplyPreset(presetName);
}

void SceneViewLayerHost::SelectEntityByName(const std::string& name)
{
    if (name.empty()) return;
    auto session = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    auto scene = session->EditorScene;
    if (!scene) return;

    CHEngine::EntityHandle found{};
    scene->ForEach<CHEngine::TagComponent>(
        [&](CHEngine::EntityHandle handle, const CHEngine::UUID&, CHEngine::TagComponent& tag)
        {
            if (tag.Name == name)
                found = handle;
        });

    if (scene->IsEntityHandleValid(found))
        session->SelectedEntity = found;
}

// ── Helper: collect entities by name (two-phase: find then modify) ────────────
// Collect EntityHandles by name — safe to call AddComponent on results afterwards.
// Entity* from TryGetEntity can become dangling after AddComponent reallocates
// entt storage; handles remain stable, so we re-resolve via TryGetEntity each time.
static std::vector<CHEngine::EntityHandle> FindHandlesByName(CHEngine::Scene* scene,
                                                              const std::string& name)
{
    std::vector<CHEngine::EntityHandle> result;
    if (!scene || name.empty()) return result;
    scene->ForEach<CHEngine::TagComponent>(
        [&](CHEngine::EntityHandle h, const CHEngine::UUID&, CHEngine::TagComponent& tag) {
            if (tag.Name == name)
                result.push_back(h);
        });
    return result;
}

void SceneViewLayerHost::SetSelectedEntityPosition(float x, float y, float z)
{
    auto session = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    auto scene   = session->EditorScene;
    if (!scene) return;
    auto* entity = scene->TryGetEntity(session->SelectedEntity);
    if (!entity || !entity->HasComponent<CHEngine::TransformComponent>()) return;
    entity->PatchComponent<CHEngine::TransformComponent>(
        [x, y, z](CHEngine::TransformComponent& tc) {
            tc.ObjectTransform.Position = { x, y, z };
        });
}

void SceneViewLayerHost::CreateAndAttachScriptWithContent(const std::string& entityName,
                                                           const std::string& luaContent)
{
    namespace fs = std::filesystem;
    Ref<ProjectManager> pm = SceneViewLayerAccess::ProjectManagerRef(m_Layer);
    if (!pm->HasProject()) return;

    Project* proj = pm->Current();
    fs::path scriptsDir = proj->ScriptsAbsPath();
    if (!fs::exists(scriptsDir)) fs::create_directories(scriptsDir);

    std::string base = entityName.empty() ? "Entity" : entityName;
    for (auto& c : base)
        if (c == ' ' || c == '/' || c == '\\' || c == ':') c = '_';

    fs::path scriptPath = scriptsDir / (base + ".lua");
    int suffix = 1;
    while (fs::exists(scriptPath))
        scriptPath = scriptsDir / (base + "_" + std::to_string(suffix++) + ".lua");

    // Write the Lua content directly
    {
        std::ofstream f(scriptPath);
        if (f) f << luaContent;
    }

    // Open in editor
    SceneViewLayerAccess::ScriptEditor(m_Layer).Open(scriptPath.string());

    // Ensure Script Editor is visible in tiling
    Sandbox::TilingManager& tiling = SceneViewLayerAccess::Tiling(m_Layer);
    if (!tiling.IsVisible(Sandbox::PanelID::ScriptEditor))
    {
        Sandbox::PanelID nearPanel = tiling.IsVisible(Sandbox::PanelID::ContentBrowser)
            ? Sandbox::PanelID::ContentBrowser : Sandbox::PanelID::Viewport;
        tiling.InsertPanel(Sandbox::PanelID::ScriptEditor, nearPanel, Sandbox::DropEdge::Right);
    }

    // Attach to the entity by name (not SelectedEntity — avoids attaching to wrong entity)
    auto session = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    auto scene   = session->EditorScene;
    if (!scene) return;

    std::string absPath = scriptPath.string();
    auto handles = FindHandlesByName(scene.get(), entityName);
    // Fallback to SelectedEntity if name not found
    if (handles.empty() && scene->IsEntityHandleValid(session->SelectedEntity))
        handles.push_back(session->SelectedEntity);

    for (auto h : handles) {
        auto* e = scene->TryGetEntity(h);
        if (!e) continue;
        if (!e->HasComponent<CHEngine::ScriptComponent>())
        {
            CHEngine::ScriptComponent sc;
            sc.Scripts.push_back(CHEngine::ScriptEntry{ absPath, true });
            e->AddComponent<CHEngine::ScriptComponent>(std::move(sc));
        }
        else
        {
            e->PatchComponent<CHEngine::ScriptComponent>(
                [&](CHEngine::ScriptComponent& sc) {
                    for (const auto& entry : sc.Scripts)
                        if (entry.Path == absPath) return;
                    sc.Scripts.push_back(CHEngine::ScriptEntry{ absPath, true });
                });
        }
    }
}

void SceneViewLayerHost::CreateAndAttachWorldScript()
{
    namespace fs = std::filesystem;
    Ref<ProjectManager> pm = SceneViewLayerAccess::ProjectManagerRef(m_Layer);
    if (!pm->HasProject()) return;

    Project* proj = pm->Current();
    fs::path scriptsDir = proj->ScriptsAbsPath();
    if (!fs::exists(scriptsDir))
        fs::create_directories(scriptsDir);

    // Базовое имя файла; дедуп через "_N".
    std::string base = "world_script";
    fs::path scriptPath = scriptsDir / (base + ".lua");
    int suffix = 1;
    while (fs::exists(scriptPath))
        scriptPath = scriptsDir / (base + "_" + std::to_string(suffix++) + ".lua");

    // Создать файл с world-шаблоном и открыть в редакторе.
    auto& editor = SceneViewLayerAccess::ScriptEditor(m_Layer);
    editor.NewWorldScript(scriptPath.string());

    // Поднять Script Editor в tiling, если его нет.
    Sandbox::TilingManager& tiling = SceneViewLayerAccess::Tiling(m_Layer);
    if (!tiling.IsVisible(Sandbox::PanelID::ScriptEditor))
    {
        Sandbox::PanelID nearPanel = tiling.IsVisible(Sandbox::PanelID::ContentBrowser)
            ? Sandbox::PanelID::ContentBrowser
            : Sandbox::PanelID::Viewport;
        tiling.InsertPanel(Sandbox::PanelID::ScriptEditor, nearPanel, Sandbox::DropEdge::Right);
    }

    // Добавить путь в Scene::WorldScripts активной сцены редактора.
    auto session = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    auto scene   = session->EditorScene;
    if (!scene) return;
    scene->WorldScripts.push_back(CHEngine::ScriptEntry{ scriptPath.string(), true });
}

void SceneViewLayerHost::CreateAndAttachWorldScriptWithContent(const std::string& luaContent)
{
    namespace fs = std::filesystem;
    Ref<ProjectManager> pm = SceneViewLayerAccess::ProjectManagerRef(m_Layer);
    if (!pm->HasProject()) return;

    Project* proj = pm->Current();
    fs::path scriptsDir = proj->ScriptsAbsPath();
    if (!fs::exists(scriptsDir))
        fs::create_directories(scriptsDir);

    std::string base = "ai_world_script";
    fs::path scriptPath = scriptsDir / (base + ".lua");
    int suffix = 1;
    while (fs::exists(scriptPath))
        scriptPath = scriptsDir / (base + "_" + std::to_string(suffix++) + ".lua");

    // Записать AI-сгенерированный код напрямую.
    {
        std::ofstream f(scriptPath);
        if (f) f << luaContent;
    }

    // Открыть в редакторе.
    SceneViewLayerAccess::ScriptEditor(m_Layer).Open(scriptPath.string());

    Sandbox::TilingManager& tiling = SceneViewLayerAccess::Tiling(m_Layer);
    if (!tiling.IsVisible(Sandbox::PanelID::ScriptEditor))
    {
        Sandbox::PanelID nearPanel = tiling.IsVisible(Sandbox::PanelID::ContentBrowser)
            ? Sandbox::PanelID::ContentBrowser
            : Sandbox::PanelID::Viewport;
        tiling.InsertPanel(Sandbox::PanelID::ScriptEditor, nearPanel, Sandbox::DropEdge::Right);
    }

    auto session = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    auto scene   = session->EditorScene;
    if (!scene) return;
    scene->WorldScripts.push_back(CHEngine::ScriptEntry{ scriptPath.string(), true });
}

void SceneViewLayerHost::SetSelectedEntityRotation(float x, float y, float z)
{
    auto session = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    auto scene   = session->EditorScene;
    if (!scene) return;
    auto* e = scene->TryGetEntity(session->SelectedEntity);
    if (!e || !e->HasComponent<CHEngine::TransformComponent>()) return;
    e->PatchComponent<CHEngine::TransformComponent>(
        [x,y,z](CHEngine::TransformComponent& tc) { tc.ObjectTransform.Rotation = {x,y,z}; });
}

void SceneViewLayerHost::SetSelectedEntityScale(float x, float y, float z)
{
    auto session = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    auto scene   = session->EditorScene;
    if (!scene) return;
    auto* e = scene->TryGetEntity(session->SelectedEntity);
    if (!e || !e->HasComponent<CHEngine::TransformComponent>()) return;
    e->PatchComponent<CHEngine::TransformComponent>(
        [x,y,z](CHEngine::TransformComponent& tc) { tc.ObjectTransform.Scale = {x,y,z}; });
}

void SceneViewLayerHost::RenameSelectedEntity(const std::string& newName)
{
    auto session = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    auto scene   = session->EditorScene;
    if (!scene || newName.empty()) return;
    auto* e = scene->TryGetEntity(session->SelectedEntity);
    if (!e || !e->HasComponent<CHEngine::TagComponent>()) return;
    e->PatchComponent<CHEngine::TagComponent>(
        [&](CHEngine::TagComponent& tag) { tag.Name = newName; });
}

void SceneViewLayerHost::FocusOnEntityByName(const std::string& name)
{
    SelectEntityByName(name);
    FocusOnSelected();
}

void SceneViewLayerHost::SetEntityPositionByName(const std::string& name,
                                                  float x, float y, float z)
{
    auto session = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    auto* scene0 = session->EditorScene.get();
    for (auto h : FindHandlesByName(scene0, name))
        if (auto* e = scene0->TryGetEntity(h); e && e->HasComponent<CHEngine::TransformComponent>())
            e->PatchComponent<CHEngine::TransformComponent>(
                [x,y,z](CHEngine::TransformComponent& tc) { tc.ObjectTransform.Position = {x,y,z}; });
}

void SceneViewLayerHost::SetEntityRotationByName(const std::string& name,
                                                  float x, float y, float z)
{
    auto session = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    auto* scene1 = session->EditorScene.get();
    for (auto h : FindHandlesByName(scene1, name))
        if (auto* e = scene1->TryGetEntity(h); e && e->HasComponent<CHEngine::TransformComponent>())
            e->PatchComponent<CHEngine::TransformComponent>(
                [x,y,z](CHEngine::TransformComponent& tc) { tc.ObjectTransform.Rotation = {x,y,z}; });
}

void SceneViewLayerHost::SetEntityScaleByName(const std::string& name,
                                               float x, float y, float z)
{
    auto session = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    auto* scene2 = session->EditorScene.get();
    for (auto h : FindHandlesByName(scene2, name))
        if (auto* e = scene2->TryGetEntity(h); e && e->HasComponent<CHEngine::TransformComponent>())
            e->PatchComponent<CHEngine::TransformComponent>(
                [x,y,z](CHEngine::TransformComponent& tc) { tc.ObjectTransform.Scale = {x,y,z}; });
}

void SceneViewLayerHost::SetEntityColorByName(const std::string& name,
                                               float r, float g, float b, float a)
{
    auto session = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    // Two-phase: collect first, then AddComponent (safe outside ForEach)
    auto* scene3 = session->EditorScene.get();
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

void SceneViewLayerHost::SetEntityVisibleByName(const std::string& name, bool visible)
{
    auto session = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    auto* scene4 = session->EditorScene.get();
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

void SceneViewLayerHost::RenameEntityByName(const std::string& oldName,
                                             const std::string& newName)
{
    if (newName.empty()) return;
    auto session = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    auto* scene5 = session->EditorScene.get();
    for (auto h : FindHandlesByName(scene5, oldName))
        if (auto* e = scene5->TryGetEntity(h); e && e->HasComponent<CHEngine::TagComponent>())
            e->PatchComponent<CHEngine::TagComponent>(
                [&](CHEngine::TagComponent& tag) { tag.Name = newName; });
}

void SceneViewLayerHost::DeleteEntityByName(const std::string& name)
{
    auto session = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    auto scene   = session->EditorScene;
    if (!scene || name.empty()) return;

    // Collect UUID first, then destroy outside ForEach
    CHEngine::UUID targetUuid{};
    bool found = false;
    scene->ForEach<CHEngine::TagComponent>(
        [&](CHEngine::EntityHandle h, const CHEngine::UUID& uuid, CHEngine::TagComponent& tag) {
            if (tag.Name == name && !found) { targetUuid = uuid; found = true; }
        });
    if (found) DestroyEntityByUuid(targetUuid);
}

void SceneViewLayerHost::SetEntityLightByName(const std::string& name,
                                               const std::string& lightType,
                                               float r, float g, float b, float intensity)
{
    auto session = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    CHEngine::LightType ltype = CHEngine::LightType::Directional;
    if (lightType == "point") ltype = CHEngine::LightType::Point;
    if (lightType == "spot")  ltype = CHEngine::LightType::Spot;

    auto* scene6 = session->EditorScene.get();
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

void SceneViewLayerHost::SetViewportFovValue(float fov_degrees)
{
    auto session = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    if (!session)
        return;
    auto* cam = session->ViewportCamera.get();
    if (!cam)
        return;
    auto& camVariant = cam->GetCamera();
    if (auto* p = std::get_if<CHEngine::PerspectiveCamera>(&camVariant))
        p->SetVerticalFOV(glm::radians(fov_degrees));
}

void SceneViewLayerHost::CreateAndAttachScriptToEntityByName(const std::string& entityName)
{
    // Find entity by name
    auto session = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    auto scene = session->EditorScene;
    if (!scene) return;

    CHEngine::EntityHandle found{};
    scene->ForEach<CHEngine::TagComponent>(
        [&](CHEngine::EntityHandle handle, const CHEngine::UUID&, CHEngine::TagComponent& tag)
        {
            if (tag.Name == entityName)
                found = handle;
        });

    if (!scene->IsEntityHandleValid(found)) return;
    CreateAndAttachScript(found, entityName);
}

void SceneViewLayerHost::OpenScriptForEntity(const std::string& entityName)
{
    auto session = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    auto scene = session->EditorScene;
    if (!scene) return;

    CHEngine::EntityHandle found{};
    scene->ForEach<CHEngine::TagComponent>(
        [&](CHEngine::EntityHandle handle, const CHEngine::UUID&, CHEngine::TagComponent& tag)
        {
            if (tag.Name == entityName)
                found = handle;
        });

    if (!scene->IsEntityHandleValid(found)) return;
    auto* entity = scene->TryGetEntity(found);
    if (!entity || !entity->HasComponent<CHEngine::ScriptComponent>()) return;

    const auto& scripts = entity->GetComponent<CHEngine::ScriptComponent>().Scripts;
    if (!scripts.empty())
        OpenScriptInEditor(scripts[0].Path);
}

void SceneViewLayerHost::OpenExportPanel()
{
    SceneViewLayerAccess::Export(m_Layer).Open();
}

std::string SceneViewLayerHost::GetSceneContextString()
{
    auto session = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    auto scene   = session->EditorScene;

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
                ss << "  - \"" << tag.Name << "\"";

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
                    if (!sc.Scripts.empty())
                        ss << " [has_script:" << sc.Scripts.size() << "]";
                }
                if (e && e->HasComponent<CHEngine::CameraComponent>())
                    ss << " [camera]";
                ss << "\n";
            });
        if (!any)
            ss << "  (scene is empty)\n";
    }

    // Editor camera info
    auto& state = session->EditorCameraState;
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

void SceneViewLayerHost::ApplyDiffuseTextureToSelectedSubmesh(size_t submesh_index, const std::string& filepath)
{
    EditorWorldContext* activeSession = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    CHEngine::Scene* scene = activeSession->EditorScene.get();
    const CHEngine::EntityHandle selectedHandle = activeSession->SelectedEntity;
    if (!scene || !scene->IsEntityHandleValid(selectedHandle))
        return;
    auto* selectedEntity = scene->TryGetEntity(selectedHandle);
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
                    std::make_shared<CHEngine::Material>(SceneViewLayerAccess::Viewport(m_Layer).GetMeshShader())));
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

void SceneViewLayerHost::ClearDiffuseTextureOnSelectedSubmesh(size_t submesh_index)
{
    EditorWorldContext* activeSession = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    CHEngine::Scene* scene = activeSession->EditorScene.get();
    const CHEngine::EntityHandle selectedHandle = activeSession->SelectedEntity;
    if (!scene || !scene->IsEntityHandleValid(selectedHandle))
        return;
    auto* selectedEntity = scene->TryGetEntity(selectedHandle);
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

void SceneViewLayerHost::ApplySpecularTextureToSelectedSubmesh(size_t submesh_index, const std::string& filepath)
{
    EditorWorldContext* activeSession = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    CHEngine::Scene* scene = activeSession->EditorScene.get();
    const CHEngine::EntityHandle selectedHandle = activeSession->SelectedEntity;
    if (!scene || !scene->IsEntityHandleValid(selectedHandle))
        return;
    auto* selectedEntity = scene->TryGetEntity(selectedHandle);
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
                    std::make_shared<CHEngine::Material>(SceneViewLayerAccess::Viewport(m_Layer).GetMeshShader())));
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

void SceneViewLayerHost::ClearSpecularTextureOnSelectedSubmesh(size_t submesh_index)
{
    EditorWorldContext* activeSession = SceneViewLayerAccess::ActiveWorldCtx(m_Layer);
    CHEngine::Scene* scene = activeSession->EditorScene.get();
    const CHEngine::EntityHandle selectedHandle = activeSession->SelectedEntity;
    if (!scene || !scene->IsEntityHandleValid(selectedHandle))
        return;
    auto* selectedEntity = scene->TryGetEntity(selectedHandle);
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

} // namespace Sandbox
