#pragma once

#include "ContentBrowserPanel.h"

#include <cstddef>
#include <vector>

class SceneViewLayer;
struct EditorWorldContext;

namespace Sandbox {
class EditorCameraController;
class EditorViewport;
class GizmoSystem;
class ProjectEditorState;
class ToolbarPanel;
class SceneHierarchyPanel;
class PropertiesPanel;
class CameraPanel;
class ProfilerPanel;
class UvEditorPanel;
class SceneBrowserPanel;
} // namespace Sandbox

/// Friend accessors for internal compilation units (host, IO, play mode, etc.).
struct SceneViewLayerAccess
{
    static EditorWorldContext& Active(SceneViewLayer& layer);
    static std::vector<EditorWorldContext>& Sessions(SceneViewLayer& layer);
    static size_t ActiveIndex(const SceneViewLayer& layer);
    static void SetActiveIndex(SceneViewLayer& layer, size_t index);

    static Sandbox::EditorCameraController& CameraController(SceneViewLayer& layer);
    static Sandbox::EditorViewport& Viewport(SceneViewLayer& layer);
    static Sandbox::GizmoSystem& Gizmo(SceneViewLayer& layer);
    static ContentBrowserPanel& ContentBrowser(SceneViewLayer& layer);

    static Sandbox::ToolbarPanel& Toolbar(SceneViewLayer& layer);
    static Sandbox::SceneHierarchyPanel& Hierarchy(SceneViewLayer& layer);
    static Sandbox::PropertiesPanel& Properties(SceneViewLayer& layer);
    static Sandbox::CameraPanel& CameraPanel(SceneViewLayer& layer);
    static Sandbox::ProfilerPanel& Profiler(SceneViewLayer& layer);
    static Sandbox::UvEditorPanel& UvEditor(SceneViewLayer& layer);
    static bool& ShowUVEditor(SceneViewLayer& layer);
    static Sandbox::ProjectEditorState& EditorState(SceneViewLayer& layer);
    static Sandbox::SceneBrowserPanel& SceneBrowser(SceneViewLayer& layer);
};
