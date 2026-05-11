#include "SceneViewLayer.h"

#include "SceneViewLayerAccess.h"
#include "SceneViewLayer_CameraOps.h"
#include "SceneViewLayer_IO.h"
#include "SceneViewLayer_ImGuiFrame.h"
#include "ProjectEditorState.h"

#include "UIThemeActive.h"

#include <CHEngine/Project/ProjectManager.h>
#include <Log/Log.h>
#include <Profiler.h>
#include <filesystem>

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

    if (CHEngine::ProjectManager::HasProject())
    {
        // Project was already loaded by SandboxApp — init editor state.
        OnProjectOpened();
        SceneViewLayerIO::TryRestoreSession(*this);
    }
}

void SceneViewLayer::OnProjectOpened()
{
    CHEngine::Project* proj = CHEngine::ProjectManager::Current();
    if (!proj)
        return;

    // Reset to a single clean session when switching projects.
    m_Sessions.resize(1);
    m_ActiveIndex = 0;
    m_Sessions[0] = EditorWorldContext{};
    SceneViewLayerCameraOps::ApplyOrbit(*this);
    m_GizmoSystem.BindCommandStack(&SceneViewLayerAccess::Active(*this).CommandStack);

    m_EditorState = Sandbox::ProjectEditorState::LoadFor(*proj);
    m_ContentBrowser.SetAssetsDirectory(proj->AssetsAbsPath());

    // Load startup scene.
    const std::string& startupRel = proj->GetStartupScene();
    if (!startupRel.empty())
    {
        const std::string absPath = proj->ResolveScenePath(startupRel);
        if (std::filesystem::exists(absPath))
            SceneViewLayerIO::LoadSceneSilent(*this, absPath);
    }
}

void SceneViewLayer::OnUpdate(CHEngine::Timestep dt)
{
    CHE_PROFILE_FUNCTION();
    if (!CHEngine::ProjectManager::HasProject())
        return;

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
    m_Viewport.EndSceneRender();
}

void SceneViewLayer::OnEvent(CHEngine::Event& e)
{
    m_CameraController.OnEvent(e);
    m_GizmoSystem.OnEvent(e);
}

void SceneViewLayer::OnImGuiRender()
{
    if (!CHEngine::ProjectManager::HasProject())
    {
        if (m_ProjectBrowser.Draw())
            OnProjectOpened();
        return;
    }
    RunSceneViewImGuiFrame(*this);
}
