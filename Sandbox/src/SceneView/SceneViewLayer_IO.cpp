#include "SceneViewLayer_IO.h"

#include "SceneViewLayer.h"
#include "SceneViewLayerAccess.h"
#include "SceneViewLayer_CameraOps.h"
#include "SetTransformCommand.h"

#include <CHEngine/Application.h>
#include <CHEngine/Mesh/Material.h>
#include <CHEngine/Mesh/ModelLoader.h>
#include <CHEngine/Render/RenderFacade.h>
#include <CHEngine/Scene/Components.h>
#include <CHEngine/Scene/RecentFiles.h>
#include <CHEngine/Scene/SceneSerializer.h>
#include <CHEngine/Utils/FileDialog.h>
#include <FileSystem/FileSystem.h>

#include <boost/uuid/nil_generator.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <nlohmann/json.hpp>
#include <sstream>

namespace {

constexpr const char* k_SessionFile = ".che_session.chscene";
constexpr const char* k_SessionStateFile = ".che_session_state";

CHEngine::RecentFiles& RecentFilesStore()
{
    static CHEngine::RecentFiles s_RecentFiles;
    return s_RecentFiles;
}

int HexToNibble(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F')
        return 10 + (c - 'A');
    return -1;
}

bool TryParseUUIDString(const std::string& input, CHEngine::UUID& outUUID)
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

    CHEngine::UUID parsed = boost::uuids::nil_uuid();
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

            for (CHEngine::Mesh& mesh : meshComponent.Meshes)
            {
                if (mesh.GetVertexBuffer().IsValid() && mesh.GetIndexBuffer().IsValid())
                    ++validVaoCount;

                if (!mesh.Mat)
                    mesh.Mat = CHEngine::MaterialInstance::FromBase(
                        std::make_shared<CHEngine::Material>(CHEngine::RenderFacade::GetDefaultMeshShader()));

                const CHEngine::ShaderHandle shaderHandle = mesh.Mat->GetMaterial()->GetShaderHandle();
                if (shaderHandle.IsValid())
                    ++validShaderCount;
            }

            const bool canSubmit = visibility.Visible && !meshComponent.Meshes.empty() && validVaoCount > 0
                && validShaderCount > 0;

            CHE_CORE_WARN(
                "SceneViewLayer: post-load entity='{}' uuid={} visible={} meshes={} validShaders={} validVaos={} renderSubmitReady={}",
                tag.Name,
                boost::uuids::to_string(uuid),
                visibility.Visible,
                meshComponent.Meshes.size(),
                validShaderCount,
                validVaoCount,
                canSubmit);
        });

    CHE_CORE_WARN("SceneViewLayer: post-load render readiness audit finished");
}

} // namespace

void SceneViewLayerIO::LoadRecentFilesList()
{
    RecentFilesStore().LoadFromFile("recent_scenes.txt");
}

void SceneViewLayerIO::SaveScene(SceneViewLayer& layer)
{
    const char* filters[] = { "*.chscene" };
    std::string path        = CHEngine::FileDialog::SaveFile("Save Scene", "scene.chscene", filters, 1, ".chscene");
    if (path.empty())
        return;

    EditorWorldContext& ctx = SceneViewLayerAccess::Active(layer);
    Sandbox::EditorCameraState& camera_state = ctx.EditorCameraState;

    CHEngine::SceneSerializer serializer{};
    auto scene_ref = ctx.EditorScene;
    auto* viewport_camera = ctx.ViewportCamera.get();
    if (!scene_ref || !viewport_camera)
        return;
    nlohmann::json sceneJson = serializer.SerializeToJson(scene_ref);

    const glm::vec3 cameraPosition = viewport_camera->GetPosition();
    sceneJson["meta"]["editorCamera"] = {
        { "position", { cameraPosition.x, cameraPosition.y, cameraPosition.z } },
        { "yaw", glm::degrees(viewport_camera->GetYaw()) },
        { "pitch", glm::degrees(viewport_camera->GetPitch()) },
        { "fov", viewport_camera->GetFOV() },
        { "orbitTarget", { camera_state.OrbitTarget.x, camera_state.OrbitTarget.y, camera_state.OrbitTarget.z } },
        { "orbitDist", camera_state.OrbitDist },
        { "followObject", camera_state.FollowObject }
    };

    if (CHEngine::FileSystem::WriteFileText(path, sceneJson.dump(4)))
    {
        RecentFilesStore().AddPath(path);
        RecentFilesStore().SaveToFile("recent_scenes.txt");
        CHE_CORE_INFO("Scene saved: {}", path);
    }
    else
        CHE_CORE_ERROR("SceneViewLayer: failed to save scene {}", path);
}

void SceneViewLayerIO::LoadScene(SceneViewLayer& layer, const std::string& path)
{
    std::string filePath = path;
    if (filePath.empty())
    {
        const char* filters[] = { "*.chscene" };
        filePath = CHEngine::FileDialog::OpenFile("Scene Files (*.chscene)", filters, 1, "Open Scene");
    }
    if (filePath.empty())
        return;

    EditorWorldContext& ctx = SceneViewLayerAccess::Active(layer);
    ctx.CommandStack.Clear();
    ctx.SelectedEntity = {};

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

        auto loadedScene = serializer.DeserializeFromJson(sceneJson);
        if (!loadedScene)
            return;
        CHE_CORE_ASSERT(ctx.EditorScene, "SceneViewLayer: EditorScene must exist");
        *ctx.EditorScene = std::move(*loadedScene);
        LogSceneRenderReadiness(ctx.EditorScene.get());
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
                auto* viewport_camera = ctx.ViewportCamera.get();
                if (!viewport_camera)
                    return;
                glm::vec3 savedPosition = viewport_camera->GetPosition();
                glm::vec3 savedOrbitTarget = ctx.EditorCameraState.OrbitTarget;
                bool hasPosition = false;
                bool hasOrbitTarget = false;

                hasPosition = tryReadVec3(cameraMeta.value("position", nlohmann::json{}), &savedPosition);
                hasOrbitTarget = tryReadVec3(cameraMeta.value("orbitTarget", nlohmann::json{}), &savedOrbitTarget);

                viewport_camera->SetYaw(glm::radians(cameraMeta.value("yaw", glm::degrees(viewport_camera->GetYaw()))));
                viewport_camera->SetPitch(
                    glm::radians(cameraMeta.value("pitch", glm::degrees(viewport_camera->GetPitch()))));
                viewport_camera->SetFOV(cameraMeta.value("fov", viewport_camera->GetFOV()));
                ctx.EditorCameraState.OrbitDist =
                    cameraMeta.value("orbitDist", ctx.EditorCameraState.OrbitDist);
                ctx.EditorCameraState.FollowObject =
                    cameraMeta.value("followObject", ctx.EditorCameraState.FollowObject);

                if (hasOrbitTarget)
                {
                    ctx.EditorCameraState.OrbitTarget = savedOrbitTarget;
                    SceneViewLayerCameraOps::ApplyOrbit(layer);
                }
                else if (hasPosition)
                    viewport_camera->SetPosition(savedPosition);
            }
        }

        RecentFilesStore().AddPath(filePath);
        RecentFilesStore().SaveToFile("recent_scenes.txt");
        CHE_CORE_INFO("Scene loaded: {}", filePath);
    }
}

void SceneViewLayerIO::AutoSaveForRestart(SceneViewLayer& layer)
{
    CHEngine::SceneSerializer serializer{};
    EditorWorldContext& activeSession = SceneViewLayerAccess::Active(layer);
    CHE_CORE_ASSERT(activeSession.EditorScene, "SceneViewLayer: EditorScene must exist");
    auto scene_ref = activeSession.EditorScene;
    auto* viewport_camera = activeSession.ViewportCamera.get();
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
    oss << viewport_camera->GetFOV() << "\n";

    const glm::vec3 orbitTarget = activeSession.EditorCameraState.OrbitTarget;
    oss << orbitTarget.x << " " << orbitTarget.y << " " << orbitTarget.z << "\n";
    oss << activeSession.EditorCameraState.OrbitDist << "\n";
    oss << (activeSession.EditorCameraState.FollowObject ? 1 : 0) << "\n";

    if (activeSession.EditorScene && scene_ref->IsEntityHandleValid(activeSession.SelectedEntity))
        oss << boost::uuids::to_string(scene_ref->GetUUID(activeSession.SelectedEntity)) << "\n";
    else
        oss << boost::uuids::to_string(boost::uuids::nil_uuid()) << "\n";

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

void SceneViewLayerIO::TryRestoreSession(SceneViewLayer& layer)
{
    if (std::filesystem::exists(k_SessionFile))
    {
        EditorWorldContext& activeSession = SceneViewLayerAccess::Active(layer);

        CHEngine::SceneSerializer serializer{};
        {
            auto loadedScene = serializer.LoadFromFile(k_SessionFile);
            if (loadedScene)
            {
                CHE_CORE_ASSERT(activeSession.EditorScene, "SceneViewLayer: EditorScene must exist");
                *activeSession.EditorScene = std::move(*loadedScene);
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
    EditorWorldContext& activeSession2 = SceneViewLayerAccess::Active(layer);
    auto* viewport_camera = activeSession2.ViewportCamera.get();
    if (!viewport_camera)
        return;
    viewport_camera->SetPosition(pos);

    float yaw, pitch, fov;
    f >> yaw >> pitch >> fov;
    viewport_camera->SetYaw(glm::radians(yaw));
    viewport_camera->SetPitch(glm::radians(pitch));
    viewport_camera->SetFOV(fov);

    glm::vec3 orbitTarget{};
    f >> orbitTarget.x >> orbitTarget.y >> orbitTarget.z;
    activeSession2.EditorCameraState.OrbitTarget = orbitTarget;
    float orbitDist = activeSession2.EditorCameraState.OrbitDist;
    f >> orbitDist;
    activeSession2.EditorCameraState.OrbitDist = orbitDist;
    int follow;
    f >> follow;
    activeSession2.EditorCameraState.FollowObject = (follow != 0);

    std::string selectedUUIDStr;
    f >> selectedUUIDStr;
    {
        CHEngine::UUID selectedUUID = boost::uuids::nil_uuid();
        if (TryParseUUIDString(selectedUUIDStr, selectedUUID) && activeSession2.EditorScene)
            activeSession2.SelectedEntity = activeSession2.EditorScene->TryGetEntityHandleByUUID(selectedUUID);
        else
            activeSession2.SelectedEntity = {};
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

void SceneViewLayerIO::ImportModel(SceneViewLayer& layer, const std::string& filepath)
{
    auto result = CHEngine::ModelLoader::Load(filepath, CHEngine::RenderFacade::GetDefaultMeshShader());
    if (!result.success)
    {
        CHE_CORE_ERROR("SceneViewLayer: failed to import model '{}': {}",
                       filepath,
                       result.error.empty() ? "unknown loader error" : result.error);
        return;
    }

    glm::vec3 centroid(0.0f);
    size_t totalVerts = 0;
    for (auto& mesh : result.meshes)
    {
        for (const auto& v : mesh.GetVertices())
        {
            centroid += v.Position;
            ++totalVerts;
        }
    }
    if (totalVerts > 0)
        centroid /= static_cast<float>(totalVerts);

    if (totalVerts > 0 && glm::length(centroid) > 1e-5f)
    {
        for (auto& mesh : result.meshes)
        {
            // Old VBO/IBO are leaked here until shutdown — acceptable in this
            // transitional phase; will be fixed when Mesh tracks buffer ownership.
            auto verts = mesh.GetVertices();
            for (auto& v : verts)
                v.Position -= centroid;
            mesh.Build(verts, mesh.GetIndices());
        }
    }

    const CHEngine::UUID objectID = boost::uuids::random_generator()();
    EditorWorldContext& activeSession = SceneViewLayerAccess::Active(layer);
    CHE_CORE_ASSERT(activeSession.EditorScene, "SceneViewLayer: EditorScene must exist");
    auto scene_ref = activeSession.EditorScene;
    if (!scene_ref)
        return;
    const CHEngine::EntityHandle handle = scene_ref->CreateEntity(result.name, objectID);
    auto* entity = scene_ref->TryGetEntity(handle);
    if (!entity || !entity->HasComponent<CHEngine::TransformComponent>() || !entity->HasComponent<CHEngine::MeshComponent>())
        return;
    entity->PatchComponent<CHEngine::MeshComponent>([&](CHEngine::MeshComponent& mesh_component) {
        mesh_component.Meshes = std::move(result.meshes);
        mesh_component.SourcePath = filepath;
    });
    entity->PatchComponent<CHEngine::TransformComponent>([&](CHEngine::TransformComponent& transform_component) {
        transform_component.ObjectTransform.Position = centroid;
    });
    activeSession.SelectedEntity = handle;
    activeSession.CommandStack.Push(CHEngine::MakeScope<Sandbox::CallbackCommand>(
        []() {},
        [scene_ref, objectID, handle, &activeSession]()
        {
            if (activeSession.SelectedEntity == handle)
                activeSession.SelectedEntity = {};
            if (scene_ref)
                scene_ref->DestroyEntity(objectID);
        },
        true));
    SceneViewLayerCameraOps::FocusOnSelected(layer);
}
