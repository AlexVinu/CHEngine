#include "SceneViewLayer.h"

#include "SceneViewLayerAccess.h"
#include "SceneViewLayer_CameraOps.h"
#include "SceneViewLayer_IO.h"
#include "SceneViewLayer_ImGuiFrame.h"

#include "UIThemeActive.h"

#include <Log/Log.h>
#include <Profiler.h>

SceneViewLayer::SceneViewLayer()
    : Layer("SceneView")
{
    m_Sessions.reserve(16);
    m_Sessions.emplace_back();

    m_GizmoSystem.BindCommandStack(&SceneViewLayerAccess::Active(*this).CommandStack);

    UIActive::SetTheme(AppTheme::RetroOS);
    UIActive::SyncLayout();

    SceneViewLayerCameraOps::ApplyOrbit(*this);
    SceneViewLayerIO::LoadRecentFilesList();
    SceneViewLayerIO::TryRestoreSession(*this);
}

void SceneViewLayer::OnUpdate(CHEngine::Timestep dt)
{
    CHE_PROFILE_FUNCTION();
    std::vector<EditorWorldContext>& sessions = SceneViewLayerAccess::Sessions(*this);
    if (sessions.empty())
        return;

    const size_t active_index = SceneViewLayerAccess::ActiveIndex(*this);
    if (active_index >= sessions.size())
        return;

    for (size_t i = 0; i < sessions.size(); ++i)
    {
        sessions[i].IsActive = false;
        if (i == active_index)
        {
            sessions[i].IsActive = true;
        }
    }

    for (size_t i = 0; i < sessions.size(); ++i)
    {
        if (i != active_index)
            sessions[i].Update(dt);
    }

    EditorWorldContext* active = &sessions[active_index];
    m_GizmoSystem.BindCommandStack(&active->CommandStack);
    m_Viewport.BeginSceneRender(active);
    active->Update(dt);
    m_Viewport.DrawEditorOverlays(active);
    m_Viewport.EndSceneRender();
}

void SceneViewLayer::OnEvent(CHEngine::Event& e)
{
    m_CameraController.OnEvent(e);
    m_GizmoSystem.OnEvent(e);
}

void SceneViewLayer::OnImGuiRender()
{
    RunSceneViewImGuiFrame(*this);
}
