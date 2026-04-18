#include "SceneViewLayer.h"

#include "SceneViewLayerAccess.h"
#include "SceneViewLayer_CameraOps.h"
#include "SceneViewLayer_IO.h"
#include "SceneViewLayer_ImGuiFrame.h"

#include "UIThemeActive.h"

#include <Log/Log.h>
#include <Profiler.h>

namespace {

void TickNonActiveSessions(SceneViewLayer& layer, CHEngine::Timestep dt)
{
    std::vector<EditorWorldContext>& sessions = SceneViewLayerAccess::Sessions(layer);
    const size_t active = SceneViewLayerAccess::ActiveIndex(layer);
    for (size_t i = 0; i < sessions.size(); ++i)
    {
        sessions[i].RuntimeWorld->SetActive(i == active);
        if (i != active)
            sessions[i].UpdateSimulation(dt);
    }
}

} // namespace

SceneViewLayer::SceneViewLayer()
    : Layer("SceneView")
{
    m_Sessions.reserve(16);
    m_Sessions.emplace_back();

    m_GizmoSystem.BindCommandStack(&SceneViewLayerAccess::Active(*this).m_CommandStack);
    m_Viewport.SetSessionUpdate([](SceneSession* session, CHEngine::Timestep dt) {
        if (session)
            static_cast<EditorWorldContext*>(session)->UpdateSimulation(dt);
    });

    UIActive::SetTheme(AppTheme::RetroOS);
    UIActive::SyncLayout();

    SceneViewLayerCameraOps::ApplyOrbit(*this);
    SceneViewLayerIO::LoadRecentFilesList();
    SceneViewLayerIO::TryRestoreSession(*this);
}

void SceneViewLayer::OnUpdate(CHEngine::Timestep dt)
{
    CHE_PROFILE_FUNCTION();
    TickNonActiveSessions(*this, dt);
    EditorWorldContext& active = SceneViewLayerAccess::Active(*this);
    m_GizmoSystem.BindCommandStack(&active.m_CommandStack);
    m_Viewport.Render(&active, dt);
}

void SceneViewLayer::OnEvent(CHEngine::Event& e)
{
    m_Camera.OnEvent(e);
    m_GizmoSystem.OnEvent(e);
}

void SceneViewLayer::OnImGuiRender()
{
    RunSceneViewImGuiFrame(*this);
}
