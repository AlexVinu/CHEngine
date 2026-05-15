#pragma once

#include <CHEngine/Layer/Layer.h>
#include <CheStl/MemoryTypes.h>

#include "ContentBrowserPanel.h"
#include "CameraPanel.h"
#include "EditorCameraController.h"
#include "EditorViewport.h"
#include "EditorWorldContext.h"
#include "ProjectManager.h"
#include "GizmoSystem.h"
#include "ProfilerPanel.h"
#include "PropertiesPanel.h"
#include "SceneBrowserPanel.h"
#include "SceneHierarchyPanel.h"
#include "ToolbarPanel.h"
#include "UvEditorPanel.h"
#include "ProjectBrowserWindow.h"
#include "ScriptEditorPanel.h"
#include "TilingManager.h"

#include <vector>

struct SceneViewLayerAccess;

class SceneViewLayer : public CHEngine::Layer
{
public:
    SceneViewLayer(Ref<std::vector<Ref<EditorWorldContext>>>, Ref<ProjectManager>);
    void OnUpdate(CHEngine::Timestep dt) override;
    void OnImGuiRender() override;
    void OnEvent(CHEngine::Event& e) override;

    void OnProjectOpened();

private:
    friend struct SceneViewLayerAccess;

    Ref<std::vector<Ref<EditorWorldContext>>> m_EditorWorldContexts;
    Ref<ProjectManager> m_ProjectManager;
    size_t m_ActiveIndex = 0;

    Sandbox::EditorCameraController m_CameraController;
    Sandbox::GizmoSystem m_GizmoSystem;
    Sandbox::EditorViewport m_Viewport;

    Sandbox::ToolbarPanel m_ToolbarPanel;
    Sandbox::SceneHierarchyPanel m_HierarchyPanel;
    Sandbox::PropertiesPanel m_PropertiesPanel;
    Sandbox::CameraPanel m_CameraPanel;
    Sandbox::ProfilerPanel m_ProfilerPanel;
    ContentBrowserPanel m_ContentBrowser;

    Sandbox::UvEditorPanel m_UvEditor;
    bool m_ShowUVEditor = false;
    Sandbox::SceneBrowserPanel m_SceneBrowser;

    ProjectBrowserWindow m_ProjectBrowser;
    Sandbox::ScriptEditorPanel m_ScriptEditor;

    // Tiling workspace manager
    Sandbox::TilingManager m_TilingManager{ "tiling_layout.txt" };
};
