#include "SceneViewLayerAccess.h"

#include "SceneViewLayer.h"
#include "TilingManager.h"
#include "UvEditorPanel.h"

EditorWorldContext* SceneViewLayerAccess::ActiveWorldCtx(SceneViewLayer& layer)
{
    return static_cast<EditorWorldContext*>(layer.m_Worlds->GetForIndex(layer.m_ActiveIndex));
}

Ref<ProjectManager> SceneViewLayerAccess::ProjectManagerRef(SceneViewLayer& layer)
{
    return layer.m_ProjectManager;
}

size_t SceneViewLayerAccess::ActiveIndex(const SceneViewLayer& layer)
{
    return layer.m_ActiveIndex;
}

void SceneViewLayerAccess::SetActiveIndex(SceneViewLayer& layer, size_t index)
{
    CHE_ASSERT(index < layer.m_Worlds->Size(), "Invalid session index");

    // Deactivate old active only when its index is still valid (it may have been erased).
    if (layer.m_ActiveIndex < layer.m_Worlds->Size())
        ActiveWorldCtx(layer)->UpdateState(std::nullopt, false);

    layer.m_ActiveIndex = index;
    ActiveWorldCtx(layer)->UpdateState(std::nullopt, true);
}

Sandbox::EditorCameraController& SceneViewLayerAccess::CameraController(SceneViewLayer& layer)
{
    return layer.m_CameraController;
}

Sandbox::EditorViewport& SceneViewLayerAccess::Viewport(SceneViewLayer& layer)
{
    return layer.m_Viewport;
}

Sandbox::GizmoSystem& SceneViewLayerAccess::Gizmo(SceneViewLayer& layer)
{
    return layer.m_GizmoSystem;
}

ContentBrowserPanel& SceneViewLayerAccess::ContentBrowser(SceneViewLayer& layer)
{
    return layer.m_ContentBrowser;
}

Sandbox::ToolbarPanel& SceneViewLayerAccess::Toolbar(SceneViewLayer& layer)
{
    return layer.m_ToolbarPanel;
}

Sandbox::SceneHierarchyPanel& SceneViewLayerAccess::Hierarchy(SceneViewLayer& layer)
{
    return layer.m_HierarchyPanel;
}

Sandbox::PropertiesPanel& SceneViewLayerAccess::Properties(SceneViewLayer& layer)
{
    return layer.m_PropertiesPanel;
}

Sandbox::CameraPanel& SceneViewLayerAccess::CameraPanel(SceneViewLayer& layer)
{
    return layer.m_CameraPanel;
}

Sandbox::ProfilerPanel& SceneViewLayerAccess::Profiler(SceneViewLayer& layer)
{
    return layer.m_ProfilerPanel;
}

Sandbox::UvEditorPanel& SceneViewLayerAccess::UvEditor(SceneViewLayer& layer)
{
    return layer.m_UvEditor;
}

Sandbox::SceneBrowserPanel& SceneViewLayerAccess::SceneBrowser(SceneViewLayer& layer)
{
    return layer.m_SceneBrowser;
}

Sandbox::ScriptEditorPanel& SceneViewLayerAccess::ScriptEditor(SceneViewLayer& layer)
{
    return layer.m_ScriptEditor;
}

Sandbox::TilingManager& SceneViewLayerAccess::Tiling(SceneViewLayer& layer)
{
    return layer.m_TilingManager;
}

Sandbox::GlobalAiOverlay& SceneViewLayerAccess::GlobalAi(SceneViewLayer& layer)
{
    return layer.m_GlobalAiOverlay;
}

Sandbox::ExportPanel& SceneViewLayerAccess::Export(SceneViewLayer& layer)
{
    return layer.m_ExportPanel;
}

CHEngine::WorldsList* SceneViewLayerAccess::WorldsList(SceneViewLayer& layer)
{
    return layer.m_Worlds.get();
}
