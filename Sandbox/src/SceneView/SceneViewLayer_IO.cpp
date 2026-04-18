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
#include <CHEngine/World/ISystem.h>
#include <CHEngine/World/WorldEvents.h>
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
                if (mesh.GetVertexArray().IsValid())
                    ++validVaoCount;

                if (!mesh.Mat)
                    mesh.Mat = CHEngine::MaterialInstance::FromBase(
                        std::make_shared<CHEngine::Material>(CHEngine::RenderFacade::GetDefaultMeshShader()));

                const CHEngine::ShaderHandle shaderHandle = mesh.Mat->GetMaterial()->GetShaderHandle();
                if (CHEngine::RenderFacade::GetShader(shaderHandle))
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
    Sandbox::EditorCamera& orbitCam = SceneViewLayerAccess::Camera(layer);

    CHEngine::SceneSerializer serializer{};
    auto scene = ctx.ActiveScene;
    CHEngine::EditorCamera& viewportCamera = *ctx.ViewportCamera;
    nlohmann::json sceneJson = serializer.SerializeToJson(scene.get());

    const glm::vec3 cameraPosition = viewportCamera.GetPosition();
    sceneJson["meta"]["editorCamera"] = {
        { "position", { cameraPosition.x, cameraPosition.y, cameraPosition.z } },
        { "yaw", glm::degrees(viewportCamera.GetYaw()) },
        { "pitch", glm::degrees(viewportCamera.GetPitch()) },
        { "fov", viewportCamera.GetFOV() },
        { "orbitTarget", { orbitCam.GetOrbitTarget().x, orbitCam.GetOrbitTarget().y, orbitCam.GetOrbitTarget().z } },
        { "orbitDist", orbitCam.GetOrbitDist() },
        { "followObject", orbitCam.GetFollowObject() }
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
    ctx.m_CommandStack.Clear();
    ctx.SelectedEntity = {};

    if (!ctx.RuntimeWorld)
        ctx.RuntimeWorld = CHEngine::MakeScope<CHEngine::World>(ctx.EditorScene.get());
    CHEngine::World& runtimeWorld = *ctx.RuntimeWorld;
    runtimeWorld.GetEvents().Publish<CHEngine::DestroyPhysicsWorldEvent>(CHEngine::SystemPhase::Simulation);
    runtimeWorld.Update(CHEngine::Timestep(0.0f));

    CHEngine::SceneSerializer serializer{};
    auto* resources = CHEngine::Application::Get().GetRenderResources();
    if (resources)
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

        CHEngine::Scope<CHEngine::Scene> loadedScene = serializer.DeserializeFromJson(sceneJson, *resources);
        if (!loadedScene)
            return;
        ctx.EditorScene = CHEngine::CreateRef<CHEngine::Scene>(std::move(*loadedScene));
        ctx.ActiveScene = ctx.EditorScene;
        CHE_CORE_ASSERT(ctx.EditorScene, "SceneViewLayer: EditorScene must exist");
        runtimeWorld.SetScene(ctx.EditorScene.get());
        LogSceneRenderReadiness(ctx.EditorScene.get());
        ctx.m_EditorCameraEntity = {};

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

        Sandbox::EditorCamera& orbitCam = SceneViewLayerAccess::Camera(layer);

        if (sceneJson.contains("meta") && sceneJson["meta"].is_object())
        {
            const auto& metaJson = sceneJson["meta"];
            if (metaJson.contains("editorCamera") && metaJson["editorCamera"].is_object())
            {
                const auto& cameraMeta = metaJson["editorCamera"];
                CHEngine::EditorCamera& viewportCamera = *ctx.ViewportCamera;
                glm::vec3 savedPosition = viewportCamera.GetPosition();
                glm::vec3 savedOrbitTarget = orbitCam.GetOrbitTarget();
                bool hasPosition = false;
                bool hasOrbitTarget = false;

                hasPosition = tryReadVec3(cameraMeta.value("position", nlohmann::json{}), &savedPosition);
                hasOrbitTarget = tryReadVec3(cameraMeta.value("orbitTarget", nlohmann::json{}), &savedOrbitTarget);

                viewportCamera.SetYaw(glm::radians(cameraMeta.value("yaw", glm::degrees(viewportCamera.GetYaw()))));
                viewportCamera.SetPitch(glm::radians(cameraMeta.value("pitch", glm::degrees(viewportCamera.GetPitch()))));
                viewportCamera.SetFOV(cameraMeta.value("fov", viewportCamera.GetFOV()));
                orbitCam.SetOrbitDist(cameraMeta.value("orbitDist", orbitCam.GetOrbitDist()));
                orbitCam.SetFollowObject(cameraMeta.value("followObject", orbitCam.GetFollowObject()));

                if (hasOrbitTarget)
                {
                    orbitCam.SetOrbitTarget(savedOrbitTarget);
                    SceneViewLayerCameraOps::ApplyOrbit(layer);
                }
                else if (hasPosition)
                    viewportCamera.SetPosition(savedPosition);
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
    CHEngine::Scene& scene = *activeSession.EditorScene;
    CHEngine::EditorCamera& viewportCamera = *activeSession.ViewportCamera;
    if (serializer.SaveToFile(&scene, k_SessionFile))
        CHE_CORE_INFO("SceneViewLayer: scene autosaved to {}", k_SessionFile);
    else
        CHE_CORE_WARN("SceneViewLayer: failed to autosave scene");

    Sandbox::EditorCamera& orbitCam = SceneViewLayerAccess::Camera(layer);
    std::ostringstream oss;
    glm::vec3 pos = viewportCamera.GetPosition();
    oss << pos.x << " " << pos.y << " " << pos.z << "\n";
    oss << glm::degrees(viewportCamera.GetYaw()) << "\n";
    oss << glm::degrees(viewportCamera.GetPitch()) << "\n";
    oss << viewportCamera.GetFOV() << "\n";

    const glm::vec3 orbitTarget = orbitCam.GetOrbitTarget();
    oss << orbitTarget.x << " " << orbitTarget.y << " " << orbitTarget.z << "\n";
    oss << orbitCam.GetOrbitDist() << "\n";
    oss << (orbitCam.GetFollowObject() ? 1 : 0) << "\n";

    if (activeSession.ActiveScene && scene.IsEntityHandleValid(activeSession.SelectedEntity))
        oss << boost::uuids::to_string(scene.GetUUID(activeSession.SelectedEntity)) << "\n";
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
        if (!activeSession.RuntimeWorld)
            activeSession.RuntimeWorld = CHEngine::MakeScope<CHEngine::World>(activeSession.EditorScene.get());
        CHEngine::World& runtimeWorld = *activeSession.RuntimeWorld;
        runtimeWorld.GetEvents().Publish<CHEngine::DestroyPhysicsWorldEvent>(CHEngine::SystemPhase::Simulation);
        runtimeWorld.Update(CHEngine::Timestep(0.0f));

        CHEngine::SceneSerializer serializer{};
        auto* resources = CHEngine::Application::Get().GetRenderResources();
        if (resources)
        {
            CHEngine::Scope<CHEngine::Scene> loadedScene = serializer.LoadFromFile(k_SessionFile, *resources);
            if (loadedScene)
            {
                activeSession.EditorScene = CHEngine::CreateRef<CHEngine::Scene>(std::move(*loadedScene));
                activeSession.ActiveScene = activeSession.EditorScene;
                CHE_CORE_ASSERT(activeSession.EditorScene, "SceneViewLayer: EditorScene must exist");
                runtimeWorld.SetScene(activeSession.EditorScene.get());
                CHE_CORE_INFO("SceneViewLayer: scene restored from {}", k_SessionFile);
            }
            else
                CHE_CORE_WARN("SceneViewLayer: failed to restore scene");
        }
        else
            CHE_CORE_WARN("SceneViewLayer: failed to restore scene");
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
    CHEngine::EditorCamera& viewportCamera = *activeSession2.ViewportCamera;
    viewportCamera.SetPosition(pos);

    float yaw, pitch, fov;
    f >> yaw >> pitch >> fov;
    viewportCamera.SetYaw(glm::radians(yaw));
    viewportCamera.SetPitch(glm::radians(pitch));
    viewportCamera.SetFOV(fov);

    Sandbox::EditorCamera& orbitCam = SceneViewLayerAccess::Camera(layer);
    glm::vec3 orbitTarget{};
    f >> orbitTarget.x >> orbitTarget.y >> orbitTarget.z;
    orbitCam.SetOrbitTarget(orbitTarget);
    float orbitDist = orbitCam.GetOrbitDist();
    f >> orbitDist;
    orbitCam.SetOrbitDist(orbitDist);
    int follow;
    f >> follow;
    orbitCam.SetFollowObject(follow != 0);

    std::string selectedUUIDStr;
    f >> selectedUUIDStr;
    {
        CHEngine::UUID selectedUUID = boost::uuids::nil_uuid();
        if (TryParseUUIDString(selectedUUIDStr, selectedUUID) && activeSession2.ActiveScene)
            activeSession2.SelectedEntity = activeSession2.ActiveScene->TryGetEntityHandleByUUID(selectedUUID);
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
            CHEngine::RenderFacade::DestroyVertexArray(mesh.GetVertexArray());
            auto verts = mesh.GetVertices();
            for (auto& v : verts)
                v.Position -= centroid;
            mesh.Build(verts, mesh.GetIndices());
        }
    }

    const CHEngine::UUID objectID = boost::uuids::random_generator()();
    EditorWorldContext& activeSession = SceneViewLayerAccess::Active(layer);
    CHE_CORE_ASSERT(activeSession.EditorScene, "SceneViewLayer: EditorScene must exist");
    CHEngine::Scene& scene = *activeSession.EditorScene;
    const CHEngine::EntityHandle handle = scene.CreateEntity(result.name, objectID);
    auto* entity = scene.TryGetEntity(handle);
    if (!entity || !entity->HasComponent<CHEngine::TransformComponent>() || !entity->HasComponent<CHEngine::MeshComponent>())
        return;
    auto& meshComponent = entity->GetComponent<CHEngine::MeshComponent>();
    meshComponent.Meshes = std::move(result.meshes);
    meshComponent.SourcePath = filepath;
    entity->GetComponent<CHEngine::TransformComponent>().ObjectTransform.Position = centroid;
    activeSession.SelectedEntity = handle;
    activeSession.m_CommandStack.Push(CHEngine::MakeScope<Sandbox::CallbackCommand>(
        []() {},
        [&scene, objectID, handle, &activeSession]()
        {
            if (activeSession.SelectedEntity == handle)
                activeSession.SelectedEntity = {};
            scene.DestroyEntity(objectID);
        },
        true));
    SceneViewLayerCameraOps::FocusOnSelected(layer);
}
