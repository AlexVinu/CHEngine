#include "SceneViewLayerAccess.h"

#include "SceneViewLayer.h"

EditorWorldContext& SceneViewLayerAccess::Active(SceneViewLayer& layer)
{
    return layer.m_Sessions[layer.m_ActiveIndex];
}

std::vector<EditorWorldContext>& SceneViewLayerAccess::Sessions(SceneViewLayer& layer)
{
    return layer.m_Sessions;
}

size_t SceneViewLayerAccess::ActiveIndex(const SceneViewLayer& layer)
{
    return layer.m_ActiveIndex;
}

void SceneViewLayerAccess::SetActiveIndex(SceneViewLayer& layer, size_t index)
{
    CHE_CORE_ASSERT(index < layer.m_Sessions.size(), "Invalid session index");
    layer.m_ActiveIndex = index;
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

bool& SceneViewLayerAccess::ShowUVEditor(SceneViewLayer& layer)
{
    return layer.m_ShowUVEditor;
}
