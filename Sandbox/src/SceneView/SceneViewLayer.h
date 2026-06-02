#pragma once

#include <CHEngine/Layer/Layer.h>
#include <CHEngine/World/WorldsList.h>
#include <CheStl/MemoryTypes.h>

#include "ContentBrowserPanel.h"
#include "CameraPanel.h"
#include "EditorContext.h"
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
#include "TilingManager.h"
#include "GlobalAiOverlay.h"

#include <vector>

class SceneViewLayer : public CHEngine::Layer
{
public:
    explicit SceneViewLayer(Sandbox::EditorContext& ctx);
    void OnImGuiRender() override;

    void OnProjectOpened();
    CHEngine::WorldsList& GetWorldsList() { return *m_Ctx.Worlds; }

private:
	void RunSceneViewImGuiFrame();
    void Finish();

    Sandbox::EditorContext& m_Ctx;
    Ref<ProjectManager> m_ProjectManager;

    // Panels rendered only by SceneViewLayer; ScriptEditor/SceneBrowser/Export live in EditorContext.
    Sandbox::ToolbarPanel m_ToolbarPanel;
    Sandbox::SceneHierarchyPanel m_HierarchyPanel;
    Sandbox::PropertiesPanel m_PropertiesPanel;
    Sandbox::CameraPanel m_CameraPanel;
    Sandbox::ProfilerPanel m_ProfilerPanel;
    ContentBrowserPanel m_ContentBrowser;

    Sandbox::UvEditorPanel m_UvEditor;
    bool m_ShowUVEditor = false;

    ProjectBrowserWindow m_ProjectBrowser;

    // Global AI overlay (double-tap Z)
    Sandbox::GlobalAiOverlay m_GlobalAiOverlay;
};
