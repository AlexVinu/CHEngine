#include "SceneViewLayer.h"

#include <filesystem>
#include <fstream>


// ============================================================================
//  Scene serialization
// ============================================================================

void SceneViewLayer::SaveScene()
{
    const char* filters[] = { "*.chscene" };
    std::string path = CHEngine::FileDialog::SaveFile(
        "Save Scene", "scene.chscene", filters, 1, ".chscene");
    if (path.empty()) return;

    CHEngine::SceneSerializer serializer(&m_Scene);
    if (serializer.SaveToFile(path)) {
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
    m_SelectedObjectID = 0;

    CHEngine::SceneSerializer serializer(&m_Scene);
    if (serializer.LoadFromFile(filePath, m_Resources)) {
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
    CHEngine::SceneSerializer serializer(&m_Scene);
    if (serializer.SaveToFile(k_SessionFile))
        CHE_CORE_INFO("SceneViewLayer: scene autosaved to {}", k_SessionFile);
    else
        CHE_CORE_WARN("SceneViewLayer: failed to autosave scene");

    // 2. Состояние редактора: камера, orbit, выделение
    std::ofstream f(k_SessionStateFile);
    if (!f) {
        CHE_CORE_WARN("SceneViewLayer: cannot write session state");
        return;
    }

    // Camera
    glm::vec3 pos = m_Camera.GetPosition();
    f << pos.x        << " " << pos.y         << " " << pos.z << "\n";
    f << m_Camera.GetYaw()   << "\n";
    f << m_Camera.GetPitch() << "\n";
    f << m_Camera.GetFOV()   << "\n";

    // Orbit
    f << m_OrbitTarget.x << " " << m_OrbitTarget.y << " " << m_OrbitTarget.z << "\n";
    f << m_OrbitDist     << "\n";
    f << (m_FollowObject ? 1 : 0) << "\n";

    // Selected object
    f << m_SelectedObjectID << "\n";

    // Window position and size — чтобы новое окно открылось на том же месте
    {
        auto* win = CHEngine::Application::Get().GetWindow();
        if (win && win->GetPlatformWindow()) {
            auto* pw = win->GetPlatformWindow();
            int wx = 0, wy = 0;
            pw->GetWindowPos(wx, wy);
            f << wx << " " << wy << " "
              << pw->GetWidth() << " " << pw->GetHeight() << "\n";
        }
    }

    CHE_CORE_INFO("SceneViewLayer: editor state saved");
}

void SceneViewLayer::TryRestoreSession()
{
    // 1. Восстанавливаем сцену
    if (std::filesystem::exists(k_SessionFile)) {
        CHEngine::SceneSerializer serializer(&m_Scene);
        if (serializer.LoadFromFile(k_SessionFile, m_Resources))
            CHE_CORE_INFO("SceneViewLayer: scene restored from {}", k_SessionFile);
        else
            CHE_CORE_WARN("SceneViewLayer: failed to restore scene");
        std::filesystem::remove(k_SessionFile);
    }

    // 2. Восстанавливаем состояние редактора
    if (!std::filesystem::exists(k_SessionStateFile)) return;

    std::ifstream f(k_SessionStateFile);
    if (!f) {
        CHE_CORE_WARN("SceneViewLayer: cannot read session state");
        return;
    }

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
    f >> m_SelectedObjectID;

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
    auto result = CHEngine::ModelLoader::Load(filepath, m_Resources);
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
            m_Resources.DestroyVertexArray(mesh.GetVertexArray());
            auto verts = mesh.GetVertices();
            for (auto& v : verts) v.Position -= centroid;
            mesh.Build(m_Resources, verts, mesh.GetIndices());
        }
    }

    auto handle = m_Scene.CreateModelEntity(result.name, std::move(result.meshes), filepath);
    auto* transform = m_Scene.TryGetComponent<CHEngine::TransformComponent>(handle);
    if (!transform)
        return;
    transform->ObjectTransform.Position = centroid;
    const auto objectID = m_Scene.GetID(handle);
    m_SelectedObjectID = objectID;
    m_UndoStack.PushImport(&m_Scene, objectID, &m_SelectedObjectID);
    FocusOnSelected();
}
