#include "SceneViewLayer.h"

#include "SceneViewLayerAccess.h"
#include "SceneViewLayer_CameraOps.h"
#include "SceneViewLayer_IO.h"

#include "UIThemeActive.h"
#include "ProjectManager.h"
#include "InputSystem.h"

#include <Log/Log.h>
#include <Profiler.h>
#include <filesystem>

// Free function defined in SceneViewLayer_ImGuiFrame.cpp
void RunSceneViewImGuiFrame(SceneViewLayer& layer);


SceneViewLayer::SceneViewLayer(Ref<std::vector<Ref<EditorWorldContext>>> worlds,
                                    Ref<ProjectManager> proj_manager)
    : Layer("SceneView"), m_EditorWorldContexts(worlds), m_ProjectManager(proj_manager), m_ActiveIndex(0)
{
    auto ctx = MakeRef<EditorWorldContext>();
    m_EditorWorldContexts->emplace_back(ctx);

    m_GizmoSystem.BindCommandStack(&SceneViewLayerAccess::ActiveRef(*this)->CommandStack);

    UIActive::SetTheme(AppTheme::RetroOS);
    UIActive::SyncLayout();

    SceneViewLayerCameraOps::ApplyOrbit(*this);

    if (m_ProjectManager->HasProject())
    {
        // Project was already loaded by SandboxApp — init editor state.
        OnProjectOpened();
        SceneViewLayerIO::TryRestoreSession(*this);
    }
}

void SceneViewLayer::OnProjectOpened()
{
    Project* proj = m_ProjectManager->Current();
    if (!proj)
        return;

    // Reset to a single clean session when switching projects.
    m_EditorWorldContexts->resize(1);

    (*m_EditorWorldContexts)[0] = MakeRef<EditorWorldContext>();

    SceneViewLayerAccess::SetActiveIndex(*this, 0);

    SceneViewLayerCameraOps::ApplyOrbit(*this);
    m_GizmoSystem.BindCommandStack(&SceneViewLayerAccess::ActiveRef(*this)->CommandStack);

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

    Sandbox::GetInputSystem().BeginFrame();

    if (!m_ProjectManager->HasProject())
        return;

    if (m_EditorWorldContexts->empty())
        return;

    Ref<EditorWorldContext> active = SceneViewLayerAccess::ActiveRef(*this);
    m_GizmoSystem.BindCommandStack(&active->CommandStack);
    
    // Thats fine if we draw only one scene per update
    // TODO: Do it in proper way
    m_Viewport.BeginSceneRender(active);
}

void SceneViewLayer::OnEvent(CHEngine::Event& e)
{
    m_CameraController.OnEvent(e);
    m_GizmoSystem.OnEvent(e);
}

void SceneViewLayer::OnImGuiRender()
{
    if (!m_ProjectManager->HasProject())
    {
        if (m_ProjectBrowser.Draw(*m_ProjectManager))
            OnProjectOpened();
        return;
    }
    RunSceneViewImGuiFrame(*this);
}
