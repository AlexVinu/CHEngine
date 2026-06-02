#include "SceneViewLayer_IO.h"

#include "EditorContext.h"
#include "SceneViewLayer_CameraOps.h"
#include "SetTransformCommand.h"
#include "ProjectManager.h"

#include <CHEngine/Application.h>
#include <CHEngine/EngineConfig.h>
#include <CHEngine/Mesh/Material.h>
#include <CHEngine/ResourceManager/ResourceManager.h>
#include <CHEngine/Scene/Components.h>
#include <CHEngine/Scene/SceneSerializer.h>
#include <CHEngine/Utils/FileDialog.h>
#include <FileSystem/FileSystem.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <nlohmann/json.hpp>
#include <magic_enum/magic_enum.hpp>
#include <sstream>

namespace {

constexpr const char* k_SessionFile = ".che_session.chscene";
constexpr const char* k_SessionStateFile = ".che_session_state";

void LogSceneRenderReadiness(CHEngine::Scene* scene)
{
    if (!scene)
        return;

    CHE_CORE_WARN("SceneViewLayer: post-load render readiness audit started");

    scene->ForEach<CHEngine::TagComponent, CHEngine::MeshComponent, CHEngine::VisibilityComponent>(
        [&](CHEngine::EntityHandle, const CHEngine::UUID& uuid, CHEngine::TagComponent& tag,
            CHEngine::MeshComponent& meshComponent, CHEngine::VisibilityComponent& visibility) {
            size_t validVaoCount = 0;
            size_t validShaderCount = 0;

            CHEngine::MeshLoader* meshLoader = CHEngine::Application::Get().Resources().GetMeshLoader();
            CHEngine::MeshRef& meshRef = meshComponent.Mesh;
            const uint32_t subCount = meshRef.IsValid() ? meshRef->GetSubMeshCount() : 0;
            if (meshRef.IsValid())
            {
                if (const auto* rec = meshLoader->GetGpuRecord(meshRef.Handle()))
                {
                    if (rec->vb.IsValid() && rec->ib.IsValid())
                        validVaoCount = subCount;
                }
                for (uint32_t s = 0; s < subCount; ++s)
                {
                    if (!meshRef->GetMaterial(s))
                        meshLoader->SetMaterial(meshRef.Handle(), s,
                            CHEngine::MaterialInstance::FromBase(std::make_shared<CHEngine::Material>(
                                CHEngine::Application::Get().Render().GetDefaultMeshShader())));
                    auto mat = meshRef->GetMaterial(s);
                    if (mat && mat->GetMaterial() && mat->GetMaterial()->GetShaderHandle().IsValid())
                        ++validShaderCount;
                }
            }

            const bool canSubmit = visibility.Visible && meshRef.IsValid() && validVaoCount > 0
                && validShaderCount > 0;

            CHE_CORE_WARN(
                "SceneViewLayer: post-load entity='{}' uuid={} visible={} submeshes={} validShaders={} validVaos={} renderSubmitReady={}",
                scene->GetString(tag.Name),
                uuid.ToString(),
                visibility.Visible,
                subCount,
                validShaderCount,
                validVaoCount,
                canSubmit);
        });

    CHE_CORE_WARN("SceneViewLayer: post-load render readiness audit finished");
}

} // namespace

void SceneViewLayerIO::LoadSceneSilent(Sandbox::EditorContext& ec, const std::string& absPath)
{
    if (absPath.empty())
        return;

    auto ctx = ec.ActiveCtx();
    ctx->CommandStack.Clear();
    ctx->SelectedEntity = {};

    Ref<ProjectManager> proj_manager = ec.Projects;
    std::filesystem::path basePath;
    if (proj_manager->HasProject())
        basePath = proj_manager->Current()->RootDir();

    CHEngine::SceneSerializer serializer{};
    auto loadedScene = serializer.LoadFromFile(absPath, basePath);
    if (!loadedScene)
    {
        CHE_CORE_ERROR("SceneViewLayerIO::LoadSceneSilent: failed to load '{}'", absPath);
        return;
    }
    CHE_CORE_ASSERT(ctx->EditorScene, "SceneViewLayer: EditorScene must exist");
    *ctx->EditorScene = std::move(*loadedScene);
    ctx->ActivateEditorScene();
    // Record the file this session is bound to.
    if (proj_manager->HasProject())
    {
        const std::string rel = proj_manager->Current()->ToRelativePath(absPath);
        ctx->SceneRelPath = std::filesystem::path(rel).generic_string();
    }
    else
        ctx->SceneRelPath = absPath;

    // Keep the Worlds index in sync with the scene name (stem only, no path/extension).
    if (!ctx->SceneRelPath.empty())
        ctx->SetWorldName(std::filesystem::path(ctx->SceneRelPath).stem().string());

    CHE_CORE_INFO("SceneViewLayerIO::LoadSceneSilent: loaded '{}'", absPath);
}

namespace {

// Pick a non-conflicting "Untitled_N.chscene" in <project>/Scenes/.
std::filesystem::path GenerateUntitledScenePath(const std::filesystem::path& scenesDir)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(scenesDir, ec);
    for (int i = 1; i < 10000; ++i)
    {
        fs::path candidate = scenesDir / ("Untitled_" + std::to_string(i) + ".chscene");
        if (!fs::exists(candidate))
            return candidate;
    }
    return scenesDir / "Untitled.chscene";
}

} // namespace

void SceneViewLayerIO::SaveScene(Sandbox::EditorContext& ec)
{
    EditorWorldContext* ctx = ec.ActiveCtx();

    // No-dialog save: write to <project>/Scenes/<name>.chscene.
    // Session's SceneRelPath is the source of truth for the destination.
    std::string path;
    Ref<ProjectManager> proj_manager = ec.Projects;
    if (proj_manager->HasProject())
    {
        Project* proj = proj_manager->Current();
        if (ctx->SceneRelPath.empty())
        {
            const std::filesystem::path gen = GenerateUntitledScenePath(proj->ScenesAbsPath());
            ctx->SceneRelPath = std::filesystem::relative(gen, proj->RootDir()).generic_string();
            path = gen.string();
        }
        else
        {
            path = (proj->RootDir() / ctx->SceneRelPath).string();
        }
    }

    Sandbox::EditorCameraState& camera_state = ctx->EditorCameraState;

    CHEngine::SceneSerializer serializer{};
    auto scene_ref = ctx->EditorScene;
    auto* viewport_camera = ctx->ViewportCamera.get();
    if (!scene_ref || !viewport_camera)
        return;
    nlohmann::json sceneJson = serializer.BuildJson(scene_ref);

    const glm::vec3 cameraPosition = viewport_camera->GetPosition();
    sceneJson["meta"]["editorCamera"] = {
        { "position", { cameraPosition.x, cameraPosition.y, cameraPosition.z } },
        { "yaw", glm::degrees(viewport_camera->GetYaw()) },
        { "pitch", glm::degrees(viewport_camera->GetPitch()) },
        { "orbitTarget", { camera_state.OrbitTarget.x, camera_state.OrbitTarget.y, camera_state.OrbitTarget.z } },
        { "orbitDist", camera_state.OrbitDist },
        { "followObject", camera_state.FollowObject },
        { "type", magic_enum::enum_name(viewport_camera->GetType())}
    };
    const auto& camera = viewport_camera->GetCamera();
    if (auto p_cam = std::get_if<CHEngine::PerspectiveCamera>(&camera))
    {
        sceneJson["meta"]["editorCamera"].merge_patch(
            { {"fov", p_cam->GetVerticalFOV() } });
    }
	else if (auto o_cam = std::get_if<CHEngine::OrthographicCamera>(&camera))
	{
		sceneJson["meta"]["editorCamera"].merge_patch(
			{ {"size", o_cam->GetSize() } });
	}

    if (CHEngine::FileSystem::WriteFileText(path, sceneJson.dump(4)))
    {
        if (proj_manager->HasProject())
        {
            proj_manager->AddRecentScene(path);
            proj_manager->Save();
        }
        CHE_CORE_INFO("Scene saved: {}", path);
    }
    else
        CHE_CORE_ERROR("SceneViewLayer: failed to save scene {}", path);
}

void SceneViewLayerIO::LoadScene(Sandbox::EditorContext& ec, const std::string& path)
{
    std::string filePath = path;
    if (filePath.empty())
    {
        const char* filters[] = { "*.chscene" };
        filePath = CHEngine::FileDialog::OpenFile("Scene Files (*.chscene)", filters, 1, "Open Scene");
    }
    if (filePath.empty())
        return;

    EditorWorldContext* ctx = ec.ActiveCtx();
    ctx->CommandStack.Clear();
    ctx->SelectedEntity = {};

    Ref<ProjectManager> proj_manager = ec.Projects;
    std::filesystem::path basePath;
    if (proj_manager->HasProject())
        basePath = proj_manager->Current()->RootDir();

    CHEngine::SceneSerializer serializer{};
    {
        const std::string sceneText = CHEngine::FileSystem::ReadFileText(filePath);
        if (sceneText.empty())
        {
            CHE_CORE_ERROR("SceneViewLayer: cannot read scene {}", filePath);
            return;
        }

        nlohmann::json sceneJson;
        try
        {
            sceneJson = nlohmann::json::parse(sceneText);
        }
        catch (const std::exception& e)
        {
            CHE_CORE_ERROR("SceneViewLayer: JSON parse error for {}: {}", filePath, e.what());
            return;
        }

        auto loadedScene = serializer.DeserializeFromJson(sceneJson, basePath);
        if (!loadedScene)
            return;
        CHE_CORE_ASSERT(ctx->EditorScene, "SceneViewLayer: EditorScene must exist");
        *ctx->EditorScene = std::move(*loadedScene);
        ctx->ActivateEditorScene();

        // Bind the session to its file so future Saves can write without a dialog.
        if (proj_manager->HasProject())
        {
            const std::string rel = proj_manager->Current()->ToRelativePath(filePath);
            ctx->SceneRelPath = std::filesystem::path(rel).generic_string();
        }
        else
            ctx->SceneRelPath = filePath;

        LogSceneRenderReadiness(ctx->EditorScene.get());
        auto tryReadVec3 = [](const nlohmann::json& value, glm::vec3* out_vec3) -> bool {
            if (!out_vec3 || !value.is_array() || value.size() < 3)
                return false;
            if (!value[0].is_number() || !value[1].is_number() || !value[2].is_number())
                return false;
            out_vec3->x = value[0].get<float>();
            out_vec3->y = value[1].get<float>();
            out_vec3->z = value[2].get<float>();
            return true;
        };

        if (sceneJson.contains("meta") && sceneJson["meta"].is_object())
        {
            const auto& metaJson = sceneJson["meta"];
            if (metaJson.contains("editorCamera") && metaJson["editorCamera"].is_object())
            {
                const auto& cameraMeta = metaJson["editorCamera"];
                auto* viewport_camera = ctx->ViewportCamera.get();
                if (!viewport_camera)
                    return;
                glm::vec3 savedPosition = viewport_camera->GetPosition();
                glm::vec3 savedOrbitTarget = ctx->EditorCameraState.OrbitTarget;
                bool hasPosition = false;
                bool hasOrbitTarget = false;

                hasPosition = tryReadVec3(cameraMeta.value("position", nlohmann::json{}), &savedPosition);
                hasOrbitTarget = tryReadVec3(cameraMeta.value("orbitTarget", nlohmann::json{}), &savedOrbitTarget);

                viewport_camera->SetYaw(glm::radians(cameraMeta.value("yaw", glm::degrees(viewport_camera->GetYaw()))));
                viewport_camera->SetPitch(
                    glm::radians(cameraMeta.value("pitch", glm::degrees(viewport_camera->GetPitch()))));

				std::string cam_type_str = cameraMeta.value("type",
					std::string(magic_enum::enum_name(viewport_camera->GetType())));
                auto cam_type = magic_enum::enum_cast<CHEngine::ECameraType>(cam_type_str);
                viewport_camera->SetCameraType(cam_type.value());
                auto& cam = viewport_camera->GetCamera();
                // Perspective
                if(auto p_cam = std::get_if<CHEngine::PerspectiveCamera>(&cam))
                    p_cam->SetVerticalFOV(cameraMeta.value("fov", p_cam->GetVerticalFOV()));

                // Ortho
				if (auto o_cam = std::get_if<CHEngine::OrthographicCamera>(&cam))
                    o_cam->SetSize(cameraMeta.value("size", o_cam->GetSize()));

                ctx->EditorCameraState.OrbitDist =
                    cameraMeta.value("orbitDist", ctx->EditorCameraState.OrbitDist);
                ctx->EditorCameraState.FollowObject =
                    cameraMeta.value("followObject", ctx->EditorCameraState.FollowObject);

                if (hasOrbitTarget)
                {
                    ctx->EditorCameraState.OrbitTarget = savedOrbitTarget;
                    SceneViewLayerCameraOps::ApplyOrbit(ec);
                }
                else if (hasPosition)
                    viewport_camera->SetPosition(savedPosition);
            }
        }


        if (proj_manager->HasProject())
        {
            proj_manager->AddRecentScene(filePath);
            proj_manager->Save();
        }

        CHE_CORE_INFO("Scene loaded: {}", filePath);
    }
}

void SceneViewLayerIO::AutoSaveForRestart(Sandbox::EditorContext& ec)
{
    CHEngine::SceneSerializer serializer{};
    EditorWorldContext* activeSession = ec.ActiveCtx();
    CHE_CORE_ASSERT(activeSession->EditorScene, "SceneViewLayer: EditorScene must exist");
    auto scene_ref = activeSession->EditorScene;
    auto* viewport_camera = activeSession->ViewportCamera.get();
    if (!scene_ref || !viewport_camera)
        return;
    if (serializer.SaveToFile(scene_ref, k_SessionFile))
        CHE_CORE_INFO("SceneViewLayer: scene autosaved to {}", k_SessionFile);
    else
        CHE_CORE_WARN("SceneViewLayer: failed to autosave scene");

    std::ostringstream oss;
    glm::vec3 pos = viewport_camera->GetPosition();
    oss << pos.x << " " << pos.y << " " << pos.z << "\n";
    oss << glm::degrees(viewport_camera->GetYaw()) << "\n";
    oss << glm::degrees(viewport_camera->GetPitch()) << "\n";
    oss << magic_enum::enum_name(viewport_camera->GetType()) << "\n";
    {
        const auto& cam = viewport_camera->GetCamera();
        if (const auto* p = std::get_if<CHEngine::PerspectiveCamera>(&cam))
            oss << p->GetVerticalFOV() << "\n";
        else if (const auto* o = std::get_if<CHEngine::OrthographicCamera>(&cam))
            oss << o->GetSize() << "\n";
        else
            oss << 0.0f << "\n";
    }

    const glm::vec3 orbitTarget = activeSession->EditorCameraState.OrbitTarget;
    oss << orbitTarget.x << " " << orbitTarget.y << " " << orbitTarget.z << "\n";
    oss << activeSession->EditorCameraState.OrbitDist << "\n";
    oss << (activeSession->EditorCameraState.FollowObject ? 1 : 0) << "\n";

    if (activeSession->EditorScene && scene_ref->IsEntityHandleValid(activeSession->SelectedEntity))
        oss << scene_ref->GetUUID(activeSession->SelectedEntity).ToString() << "\n";
    else
        oss << CHEngine::UUID::Nil().ToString() << "\n";

    {
        auto* win = CHEngine::Application::Get().GetWindow();
        if (win && win->GetPlatformWindow())
        {
            auto* pw = win->GetPlatformWindow();
            int wx = 0, wy = 0;
            pw->GetWindowPos(wx, wy);
            oss << wx << " " << wy << " " << pw->GetWidth() << " " << pw->GetHeight() << "\n";
        }
    }

    if (!CHEngine::FileSystem::WriteFileText(k_SessionStateFile, oss.str()))
    {
        CHE_CORE_WARN("SceneViewLayer: cannot write session state");
        return;
    }

    CHE_CORE_INFO("SceneViewLayer: editor state saved");
}

void SceneViewLayerIO::TryRestoreSession(Sandbox::EditorContext& ec)
{
    if (std::filesystem::exists(k_SessionFile))
    {
        auto activeSession = ec.ActiveCtx();

        Ref<ProjectManager> proj_manager = ec.Projects;
        std::filesystem::path basePath;
        if (proj_manager->HasProject())
            basePath = proj_manager->Current()->RootDir();

        CHEngine::SceneSerializer serializer{};
        {
            auto loadedScene = serializer.LoadFromFile(k_SessionFile, basePath);
            if (loadedScene)
            {
                CHE_CORE_ASSERT(activeSession->EditorScene, "SceneViewLayer: EditorScene must exist");
                *activeSession->EditorScene = std::move(*loadedScene);
                activeSession->ActivateEditorScene();
                CHE_CORE_INFO("SceneViewLayer: scene restored from {}", k_SessionFile);
            }
            else
                CHE_CORE_WARN("SceneViewLayer: failed to restore scene");
        }
        std::filesystem::remove(k_SessionFile);
    }

    if (!std::filesystem::exists(k_SessionStateFile))
        return;

    const std::string stateText = CHEngine::FileSystem::ReadFileText(k_SessionStateFile);
    if (stateText.empty())
    {
        CHE_CORE_WARN("SceneViewLayer: cannot read session state");
        return;
    }
    std::istringstream f(stateText);

    glm::vec3 pos{};
    f >> pos.x >> pos.y >> pos.z;
    auto activeSession2 = ec.ActiveCtx();
    auto* viewport_camera = activeSession2->ViewportCamera.get();
    if (!viewport_camera)
        return;
    viewport_camera->SetPosition(pos);

    float yaw, pitch;
    f >> yaw >> pitch;
    viewport_camera->SetYaw(glm::radians(yaw));
    viewport_camera->SetPitch(glm::radians(pitch));

    std::string camTypeStr;
    float camParam = 0.0f;
    f >> camTypeStr >> camParam;
    {
        auto camType = magic_enum::enum_cast<CHEngine::ECameraType>(camTypeStr);
        if (camType.has_value())
        {
            viewport_camera->SetCameraType(camType.value());
            auto& cam = viewport_camera->GetCamera();
            if (auto* p = std::get_if<CHEngine::PerspectiveCamera>(&cam))
                p->SetVerticalFOV(camParam);
            else if (auto* o = std::get_if<CHEngine::OrthographicCamera>(&cam))
                o->SetSize(camParam);
        }
    }

    glm::vec3 orbitTarget{};
    f >> orbitTarget.x >> orbitTarget.y >> orbitTarget.z;
    activeSession2->EditorCameraState.OrbitTarget = orbitTarget;
    float orbitDist = activeSession2->EditorCameraState.OrbitDist;
    f >> orbitDist;
    activeSession2->EditorCameraState.OrbitDist = orbitDist;
    int follow;
    f >> follow;
    activeSession2->EditorCameraState.FollowObject = (follow != 0);

    std::string selectedUUIDStr;
    f >> selectedUUIDStr;
    {
        CHEngine::UUID selectedUUID = CHEngine::UUID::FromString(selectedUUIDStr);
        if (selectedUUID.IsValid() && activeSession2->EditorScene)
            activeSession2->SelectedEntity = activeSession2->EditorScene->TryGetEntityHandleByUUID(selectedUUID);
        else
            activeSession2->SelectedEntity = {};
    }

    {
        int wx = -1, wy = -1;
        uint32_t ww = 0, wh = 0;
        if (f >> wx >> wy >> ww >> wh && wx >= 0 && ww > 0 && wh > 0)
        {
            auto* win = CHEngine::Application::Get().GetWindow();
            if (win && win->GetPlatformWindow())
            {
                auto* pw = win->GetPlatformWindow();
                pw->SetWindowPos(wx, wy);
                pw->SetWindowSize(ww, wh);
            }
        }
    }

    std::filesystem::remove(k_SessionStateFile);
    CHE_CORE_INFO("SceneViewLayer: editor state restored");
}

void SceneViewLayerIO::ImportModel(Sandbox::EditorContext& ec, const std::string& filepath)
{
    namespace fs = std::filesystem;

    // If a project is open and the source lives outside the project's Models folder,
    // copy it in so the project stays self-contained and the scene can store a relative path.
    std::string loadPath = filepath;        // what the loader actually reads
    std::string sourceForScene = filepath;  // what gets stored in MeshComponent::SourcePath

    Ref<ProjectManager> proj_manager = ec.Projects;
    if (proj_manager->HasProject() && !filepath.empty())
    {
        Project* proj = proj_manager->Current();
        const fs::path src = fs::absolute(fs::path(filepath));
        const fs::path modelsDir = proj->AssetsAbsPath() / "Models";

        std::error_code ec;
        fs::create_directories(modelsDir, ec);

        // Already inside the project's Models dir? Just record a relative path.
        const fs::path srcParent = src.parent_path();
        const bool insideModels = (srcParent == modelsDir);

        if (!insideModels)
        {
            // Pick a non-conflicting destination filename.
            const fs::path stem = src.stem();
            const fs::path ext  = src.extension();
            fs::path dst = modelsDir / src.filename();
            for (int i = 1; fs::exists(dst); ++i)
                dst = modelsDir / (stem.string() + "_" + std::to_string(i) + ext.string());

            fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
            if (ec)
            {
                CHE_CORE_WARN("ImportModel: failed to copy '{}' -> '{}': {}",
                              src.string(), dst.string(), ec.message());
                // Fall back to loading from the original location, but the scene
                // will then reference an external absolute path.
            }
            else
            {
                // glTF often ships with a sibling .bin — copy it too if present.
                const std::string extLower = [&]{
                    std::string s = ext.string();
                    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
                    return s;
                }();
                if (extLower == ".gltf")
                {
                    const fs::path binSrc = srcParent / (stem.string() + ".bin");
                    if (fs::exists(binSrc))
                    {
                        const fs::path binDst = dst.parent_path() / (dst.stem().string() + ".bin");
                        std::error_code ec2;
                        fs::copy_file(binSrc, binDst, fs::copy_options::overwrite_existing, ec2);
                        if (ec2)
                            CHE_CORE_WARN("ImportModel: failed to copy sidecar .bin: {}", ec2.message());
                    }
                }

                loadPath = dst.string();
            }
        }

        // Store path relative to project root, e.g. "Assets/Models/cube.obj".
        const fs::path target = insideModels ? src : fs::path(loadPath);
        std::error_code rel_ec;
        const fs::path rel = fs::relative(target, proj->RootDir(), rel_ec);
        if (!rel_ec)
            sourceForScene = rel.generic_string();
    }

    Project* proj2 = proj_manager->Current();
    const auto& abs_path = proj2 ? proj2->AssetsAbsPath() : std::filesystem::path{};

    auto modelHandle = CHEngine::Application::Get().Resources().Load<CHEngine::ModelHandle>(
        abs_path / loadPath, CHEngine::Application::Get().Render().GetDefaultMeshShader());
    const CHEngine::LoadedModel* result = modelHandle.IsValid()
        ? CHEngine::Application::Get().Resources().GetModel(modelHandle) : nullptr;

    if (!result || !result->mesh.IsValid())
    {
        CHE_CORE_ERROR("SceneViewLayer: failed to import model '{}'", filepath);
        return;
    }

    // MeshRef copy — AddRefs the shared GpuRecord in MeshLoader.
    CHEngine::MeshRef meshRefImported = result->mesh;

    CHEngine::MeshLoader* meshLoader = CHEngine::Application::Get().Resources().GetMeshLoader();
    glm::vec3 centroid(0.0f);
    size_t totalVerts = 0;
    if (const auto* rec = meshLoader->GetGpuRecord(meshRefImported.Handle()))
    {
        for (const auto& v : rec->vertices)
        {
            centroid += v.Position;
            ++totalVerts;
        }
    }
    if (totalVerts > 0)
        centroid /= static_cast<float>(totalVerts);

    if (totalVerts > 0 && glm::length(centroid) > 1e-5f)
    {
        const auto* rec = meshLoader->GetGpuRecord(meshRefImported.Handle());
        if (rec)
        {
            auto verts = rec->vertices;
            for (auto& v : verts)
                v.Position -= centroid;
            auto subs = rec->subMeshes;
            auto idx  = rec->indices;
            std::vector<Ref<CHEngine::MaterialInstance>> mats = meshRefImported->GetMaterials();
            meshRefImported = CHEngine::MeshRef{
                meshLoader->GetOrCreate(verts, idx, subs, std::move(mats))
            };
        }
    }

    const CHEngine::UUID objectID = CHEngine::UUID::Generate();
    EditorWorldContext* activeSession = ec.ActiveCtx();
    CHE_CORE_ASSERT(activeSession->EditorScene, "SceneViewLayer: EditorScene must exist");
    auto scene_ref = activeSession->EditorScene;
    if (!scene_ref)
        return;
    const CHEngine::EntityHandle handle = scene_ref->CreateEntity(result->name, objectID);
    auto* entity = scene_ref->TryGetEntity(handle);
    if (!entity)
        return;
    entity->AddComponent<CHEngine::TransformComponent>();
    entity->AddComponent<CHEngine::MeshComponent>();
    entity->PatchComponent<CHEngine::MeshComponent>([&](CHEngine::MeshComponent& mesh_component) {
        mesh_component.Mesh = std::move(meshRefImported);
        mesh_component.SourcePath = scene_ref->InternString(sourceForScene);
    });
    entity->PatchComponent<CHEngine::TransformComponent>([&](CHEngine::TransformComponent& transform_component) {
        transform_component.ObjectTransform.Position = centroid;
        transform_component.MarkDirty();
    });
    activeSession->SelectedEntity = handle;
    activeSession->CommandStack.Push(MakeScope<Sandbox::CallbackCommand>(
        []() {},
        [scene_ref, objectID, handle, &activeSession]()
        {
            if (activeSession->SelectedEntity == handle)
                activeSession->SelectedEntity = {};
            if (scene_ref)
                scene_ref->DestroyEntity(objectID);
        },
        true));
    SceneViewLayerCameraOps::FocusOnSelected(ec);
}

// ── Session (tab) management ──────────────────────────────────────────────────

std::vector<EditorWorldContext*> SceneViewLayerIO::GetSceneSessions(Sandbox::EditorContext& ec)
{
    auto* wl = ec.Worlds.get();
    std::vector<EditorWorldContext*> result;
    result.reserve(wl->Size());
    wl->ForEach([&](CHEngine::World& w) {
        result.push_back(static_cast<EditorWorldContext*>(&w));
    });
    return result;
}

void SceneViewLayerIO::AddSceneSession(Sandbox::EditorContext& ec)
{
    auto world_list = ec.Worlds.get();
    auto session = new EditorWorldContext(world_list);
    world_list->PushBack(session);

    Sandbox::EditorViewport& viewport = ec.Viewport;
    const auto active = ec.ActiveCtx();
    if (viewport.GetViewportSize().x > 1.0f && viewport.GetViewportSize().y > 1.0f)
        session->ViewportSize = { viewport.GetViewportSize().x, viewport.GetViewportSize().y };
    else
        session->ViewportSize = active->ViewportSize;
    session->ViewportCamera->SetViewportSize(session->ViewportSize.x, session->ViewportSize.y);
    session->EditorCameraState = active->EditorCameraState;
    ec.CameraController.ApplyOrbit(session->ViewportCamera.get(), session->EditorCameraState);
    ec.SetActiveIndex(world_list->Size() - 1);
}

void SceneViewLayerIO::CloseSceneSession(Sandbox::EditorContext& ec, size_t session_index)
{
    auto world_list = ec.Worlds.get();
    if (world_list->Size() <= 1)
        return; // Always keep at least one session open.
    if (session_index >= world_list->Size())
        return;

    size_t active = ec.ActiveIndex;
    world_list->Erase(session_index);

    if (active == session_index)
    {
        const size_t newIdx = (session_index > 0) ? session_index - 1 : 0;
        ec.SetActiveIndex(newIdx);
    }
    else if (active > session_index)
    {
        ec.SetActiveIndex(active - 1);
    }
}

// ── Scene file management (Scene Browser) ─────────────────────────────────────

void SceneViewLayerIO::OpenSceneFile(Sandbox::EditorContext& ec, const std::string& relOrAbsPath)
{
    if (relOrAbsPath.empty())
        return;

    namespace fs = std::filesystem;
    std::string absPath = relOrAbsPath;
    std::string rel = relOrAbsPath;

    Ref<ProjectManager> proj_manager = ec.Projects;

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
    auto world_list = ec.Worlds.get();
    for (size_t i = 0; i < world_list->Size(); ++i)
    {
        auto session = static_cast<EditorWorldContext*>(world_list->GetForIndex(i));
        if (session->SceneRelPath == rel)
        {
            ec.SetActiveIndex(i);
            return;
        }
    }

    // If the active session is empty/untitled, reuse it; otherwise open a new tab.
    auto cur = ec.ActiveCtx();
    if (!cur->SceneRelPath.empty())
        AddSceneSession(ec);

    LoadSceneSilent(ec, absPath);
}

void SceneViewLayerIO::NewSceneFile(Sandbox::EditorContext& ec)
{
    Ref<ProjectManager> proj_manager = ec.Projects;
    if (!proj_manager->HasProject())
        return;
    const std::string rel = proj_manager->Current()->CreateScene();
    if (!rel.empty())
        OpenSceneFile(ec, rel);
}

void SceneViewLayerIO::DeleteSceneFile(Sandbox::EditorContext& ec, const std::string& rel)
{
    Ref<ProjectManager> proj_manager = ec.Projects;
    if (!proj_manager->HasProject() || rel.empty())
        return;

    if (!proj_manager->Current()->DeleteScene(rel))
        return;

    // Close any tabs bound to it (but keep at least one session alive).
    auto* wl = ec.Worlds.get();
    for (size_t i = wl->Size(); i-- > 0;)
    {
        auto* s = static_cast<EditorWorldContext*>(wl->GetForIndex(i));
        if (s->SceneRelPath == rel)
        {
            if (wl->Size() > 1)
                CloseSceneSession(ec, i);
            else
                s->SceneRelPath.clear();
        }
    }
}

void SceneViewLayerIO::RenameSceneFile(Sandbox::EditorContext& ec, const std::string& oldRel, const std::string& newName)
{
    Ref<ProjectManager> proj_manager = ec.Projects;
    if (!proj_manager->HasProject() || oldRel.empty() || newName.empty())
        return;

    const std::string newRel = proj_manager->Current()->RenameScene(oldRel, newName);
    if (newRel.empty())
        return;

    // Update any open tabs.
    auto* wl = ec.Worlds.get();
    wl->ForEach([&](CHEngine::World& w) {
        auto& s = static_cast<EditorWorldContext&>(w);
        if (s.SceneRelPath == oldRel)
            s.SceneRelPath = newRel;
    });
}

void SceneViewLayerIO::SetStartupSceneFile(Sandbox::EditorContext& ec, const std::string& rel)
{
    Ref<ProjectManager> proj_manager = ec.Projects;
    if (!proj_manager->HasProject() || rel.empty())
        return;
    Project* proj = proj_manager->Current();
    proj->SetStartupScene(rel);
    proj->Save();

    // Move the matching open session (if any) to index 0. Otherwise just record the pref.
    auto* wl = ec.Worlds.get();
    for (size_t i = 0; i < wl->Size(); ++i)
    {
        auto* s = static_cast<EditorWorldContext*>(wl->GetForIndex(i));
        if (s->SceneRelPath == rel && i != 0)
        {
            wl->Swap(0, i);
            const size_t active = ec.ActiveIndex;
            if (active == 0)
                ec.SetActiveIndex(i);
            else if (active == i)
                ec.SetActiveIndex(0);
            break;
        }
    }
}

void SceneViewLayerIO::OpenImportModelDialog(Sandbox::EditorContext& ec)
{
    std::string path = CHEngine::FileDialog::OpenModelFile();
    if (!path.empty())
        ImportModel(ec, path);
}

void SceneViewLayerIO::SelectRendererApi(Sandbox::EditorContext& ec, CHEngine::ERenderAPI api)
{
    CHEngine::EngineConfig::SaveRendererPreference(api);
    AutoSaveForRestart(ec);
    CHEngine::Application::Get().RequestRestart();
}
