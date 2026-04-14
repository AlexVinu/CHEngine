#include "SceneViewLayer.h"

#include <CHEngine/Render/RenderFacade.h>
#include <CHEngine/Scene/SceneSerializer.h>
#include <CHEngine/World/ISystem.h>
#include <CHEngine/World/WorldEvents.h>
#include <FileSystem/FileSystem.h>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/nil_generator.hpp>
#include <boost/uuid/random_generator.hpp>

#include <filesystem>
#include <sstream>
#include <cstddef>
#include <memory>

namespace {
int HexToNibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
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
} // namespace

// ============================================================================
//  Scene serialization
// ============================================================================

void SceneViewLayer::SaveScene()
{
    const char* filters[] = { "*.chscene" };
    std::string path = CHEngine::FileDialog::SaveFile(
        "Save Scene", "scene.chscene", filters, 1, ".chscene");
    if (path.empty()) return;

    CHEngine::SceneSerializer serializer{};
    if (serializer.SaveToFile(&m_Scene, path)) {
        m_RecentFiles.AddPath(path);
        m_RecentFiles.SaveToFile("recent_scenes.txt");
    }
}

void SceneViewLayer::LoadScene(const std::string& path)
{
    std::string filePath = path;
    if (filePath.empty()) {
        const char* filters[] = { "*.chscene" };
        filePath = CHEngine::FileDialog::OpenFile(
            "Scene Files (*.chscene)", filters, 1, "Open Scene");
    }
    if (filePath.empty()) return;

    // Clear undo and selection
    m_UndoStack = UndoStack{};
    m_SelectedObjectID = boost::uuids::nil_uuid();

    m_World.GetEvents().Publish<CHEngine::DestroyPhysicsWorldEvent>(CHEngine::SystemPhase::Simulation);
    FlushSimulationEvents();

    CHEngine::SceneSerializer serializer{};
    auto* resources = CHEngine::Application::Get().GetRenderResources();
    if (resources && serializer.LoadFromFile(&m_Scene, filePath, *resources, &m_World)) {
        m_World.GetEvents().Publish<CHEngine::RebuildPhysicsWorldEvent>(CHEngine::SystemPhase::Simulation);
        FlushSimulationEvents();
        m_RecentFiles.AddPath(filePath);
        m_RecentFiles.SaveToFile("recent_scenes.txt");
    }
}

// ============================================================================
//  API switch session save/restore
// ============================================================================

void SceneViewLayer::AutoSaveForRestart()
{
    // 1. Сцена (объекты, источники света)
    CHEngine::SceneSerializer serializer{};
    if (serializer.SaveToFile(&m_Scene, k_SessionFile))
        CHE_CORE_INFO("SceneViewLayer: scene autosaved to {}", k_SessionFile);
    else
        CHE_CORE_WARN("SceneViewLayer: failed to autosave scene");

    // 2. Состояние редактора: камера, orbit, выделение
    std::ostringstream oss;
    // Camera
    glm::vec3 pos = m_Camera.GetPosition();
    oss << pos.x        << " " << pos.y         << " " << pos.z << "\n";
    oss << m_Camera.GetYaw()   << "\n";
    oss << m_Camera.GetPitch() << "\n";
    oss << m_Camera.GetFOV()   << "\n";

    // Orbit
    oss << m_OrbitTarget.x << " " << m_OrbitTarget.y << " " << m_OrbitTarget.z << "\n";
    oss << m_OrbitDist     << "\n";
    oss << (m_FollowObject ? 1 : 0) << "\n";

    // Selected object
    oss << boost::uuids::to_string(m_SelectedObjectID) << "\n";

    // Window position and size — чтобы новое окно открылось на том же месте
    {
        auto* win = CHEngine::Application::Get().GetWindow();
        if (win && win->GetPlatformWindow()) {
            auto* pw = win->GetPlatformWindow();
            int wx = 0, wy = 0;
            pw->GetWindowPos(wx, wy);
            oss << wx << " " << wy << " "
              << pw->GetWidth() << " " << pw->GetHeight() << "\n";
        }
    }

    if (!CHEngine::FileSystem::WriteFileText(k_SessionStateFile, oss.str())) {
        CHE_CORE_WARN("SceneViewLayer: cannot write session state");
        return;
    }

    CHE_CORE_INFO("SceneViewLayer: editor state saved");
}

void SceneViewLayer::TryRestoreSession()
{
    // 1. Восстанавливаем сцену
    if (std::filesystem::exists(k_SessionFile)) {
        m_World.GetEvents().Publish<CHEngine::DestroyPhysicsWorldEvent>(CHEngine::SystemPhase::Simulation);
        FlushSimulationEvents();

        CHEngine::SceneSerializer serializer{};
        auto* resources = CHEngine::Application::Get().GetRenderResources();
        if (resources && serializer.LoadFromFile(&m_Scene, k_SessionFile, *resources, &m_World)) {
            m_World.GetEvents().Publish<CHEngine::RebuildPhysicsWorldEvent>(CHEngine::SystemPhase::Simulation);
            FlushSimulationEvents();
            CHE_CORE_INFO("SceneViewLayer: scene restored from {}", k_SessionFile);
        }
        else
            CHE_CORE_WARN("SceneViewLayer: failed to restore scene");
        std::filesystem::remove(k_SessionFile);
    }

    // 2. Восстанавливаем состояние редактора
    if (!std::filesystem::exists(k_SessionStateFile)) return;

    const std::string stateText = CHEngine::FileSystem::ReadFileText(k_SessionStateFile);
    if (stateText.empty()) {
        CHE_CORE_WARN("SceneViewLayer: cannot read session state");
        return;
    }
    std::istringstream f(stateText);

    // Camera
    glm::vec3 pos{};
    f >> pos.x >> pos.y >> pos.z;
    m_Camera.SetPosition(pos);

    float yaw, pitch, fov;
    f >> yaw >> pitch >> fov;
    m_Camera.SetYaw(yaw);
    m_Camera.SetPitch(pitch);
    m_Camera.SetFOV(fov);

    // Orbit
    f >> m_OrbitTarget.x >> m_OrbitTarget.y >> m_OrbitTarget.z;
    f >> m_OrbitDist;
    int follow;
    f >> follow;
    m_FollowObject = (follow != 0);

    // Selected object
    std::string selectedUUIDStr;
    f >> selectedUUIDStr;
    if (!TryParseUUIDString(selectedUUIDStr, m_SelectedObjectID))
        m_SelectedObjectID = boost::uuids::nil_uuid();

    // Восстанавливаем позицию и размер окна
    {
        int wx = -1, wy = -1;
        uint32_t ww = 0, wh = 0;
        if (f >> wx >> wy >> ww >> wh && wx >= 0 && ww > 0 && wh > 0) {
            auto* win = CHEngine::Application::Get().GetWindow();
            if (win && win->GetPlatformWindow()) {
                auto* pw = win->GetPlatformWindow();
                pw->SetWindowPos(wx, wy);
                pw->SetWindowSize(ww, wh);
            }
        }
    }

    std::filesystem::remove(k_SessionStateFile);
    CHE_CORE_INFO("SceneViewLayer: editor state restored");
}

// ============================================================================
//  Model import
// ============================================================================

void SceneViewLayer::ImportModel(const std::string& filepath)
{
    auto result = CHEngine::ModelLoader::Load(filepath, CHEngine::RenderFacade::GetDefaultMeshShader());
    if (!result.success)
        return;

    // Compute geometric centroid across all meshes so the gizmo
    // snaps to the visual centre of the model.
    glm::vec3 centroid(0.0f);
    size_t    totalVerts = 0;
    for (auto& mesh : result.meshes)
    {
        for (const auto& v : mesh.GetVertices())
        {
            centroid += v.Position;
            ++totalVerts;
        }
    }
    if (totalVerts > 0) centroid /= static_cast<float>(totalVerts);

    // Re-centre vertices around local origin and rebuild GPU buffers.
    if (totalVerts > 0 && glm::length(centroid) > 1e-5f)
    {
        for (auto& mesh : result.meshes)
        {
            CHEngine::RenderFacade::DestroyVertexArray(mesh.GetVertexArray());
            auto verts = mesh.GetVertices();
            for (auto& v : verts) v.Position -= centroid;
            mesh.Build(verts, mesh.GetIndices());
        }
    }

    CHEngine::DeferredOps* deferred_ops = &m_World.GetDeferredOps();
    const CHEngine::UUID objectID = boost::uuids::random_generator()();
    deferred_ops->CreateEntityWithUUID(result.name, objectID);
    auto meshPayload = std::make_shared<std::vector<CHEngine::Mesh>>(std::move(result.meshes));
    deferred_ops->Enqueue([objectID, meshPayload, filepath](CHEngine::Scene* scene)
    {
        CHE_CORE_ASSERT(scene, "ImportModel deferred callback expects valid Scene");
        const CHEngine::EntityHandle handle = scene->TryGetEntityHandleByUUID(objectID);
        if (!scene->IsEntityHandleValid(handle))
            return;

        CHEngine::Entity* entity = scene->TryGetEntity(handle);
        if (!entity || !entity->HasComponent<CHEngine::MeshComponent>())
            return;

        auto& meshComponent = entity->GetComponent<CHEngine::MeshComponent>();
        meshComponent.Meshes = std::move(*meshPayload);
        meshComponent.SourcePath = filepath;
    });
    m_World.Update(CHEngine::Timestep(0.0f));
    auto handle = m_Scene.TryGetEntityHandleByUUID(objectID);
    auto* entity = m_Scene.TryGetEntity(handle);
    if (!entity || !entity->HasComponent<CHEngine::TransformComponent>())
        return;
    entity->GetComponent<CHEngine::TransformComponent>().ObjectTransform.Position = centroid;
    m_SelectedObjectID = objectID;
    m_UndoStack.PushImport(&m_Scene, objectID, &m_SelectedObjectID);
    FocusOnSelected();
}
