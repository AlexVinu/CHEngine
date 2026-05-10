#pragma once

#include <CHEngine/Layer/Layer.h>

#include "ContentBrowserPanel.h"
#include "CameraPanel.h"
#include "EditorCameraController.h"
#include "EditorViewport.h"
#include "EditorWorldContext.h"
#include "GizmoSystem.h"
#include "ProfilerPanel.h"
#include "PropertiesPanel.h"
#include "SceneHierarchyPanel.h"
#include "ToolbarPanel.h"

#include <vector>

struct SceneViewLayerAccess;

class SceneViewLayer : public CHEngine::Layer
{
public:
    SceneViewLayer();
    void OnUpdate(CHEngine::Timestep dt) override;
    void OnImGuiRender() override;
    void OnEvent(CHEngine::Event& e) override;

private:
    friend struct SceneViewLayerAccess;

    std::vector<EditorWorldContext> m_Sessions;
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
};
