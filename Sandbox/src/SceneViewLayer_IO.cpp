#include "SceneViewLayer.h"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

// ============================================================================
//  Scene serialization
// ============================================================================

void SceneViewLayer::SaveScene()
{
    auto& res = CHEngine::Application::Get().GetRenderResources();
    (void)res;

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
    auto& res = CHEngine::Application::Get().GetRenderResources();

    std::string filePath = path;
    if (filePath.empty()) {
        filePath = CHEngine::FileDialog::OpenFile("Scene Files (*.chscene)", "*.chscene");
    }
    if (filePath.empty()) return;

    // Clear undo and selection
    m_UndoStack = UndoStack{};
    m_SelectedObjectID = 0;

    CHEngine::SceneSerializer serializer(&m_Scene);
    if (serializer.LoadFromFile(filePath, res)) {
        m_RecentFiles.AddPath(filePath);
        m_RecentFiles.SaveToFile("recent_scenes.txt");
    }
}

// ============================================================================
//  Model import
// ============================================================================

void SceneViewLayer::ImportModel(const std::string& filepath)
{
    auto& res    = CHEngine::Application::Get().GetRenderResources();
    auto  result = CHEngine::ModelLoader::Load(filepath, res);
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
            res.DestroyVertexArray(mesh.GetVertexArray());
            auto verts = mesh.GetVertices();
            for (auto& v : verts) v.Position -= centroid;
            mesh.Build(res, verts, mesh.GetIndices());
        }
    }

    auto* obj = m_Scene.AddModel(result.name, std::move(result.meshes), filepath);
    obj->ObjectTransform.Position = centroid;
    m_SelectedObjectID = obj->ID;
    m_UndoStack.PushImport(&m_Scene, obj->ID, &m_SelectedObjectID);
    FocusOnSelected();
}
