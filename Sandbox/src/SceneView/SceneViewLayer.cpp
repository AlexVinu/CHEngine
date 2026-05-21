#include "SceneViewLayer.h"

#include "SceneViewLayerAccess.h"
#include "SceneViewLayer_CameraOps.h"
#include "SceneViewLayer_IO.h"

#include "UIThemeActive.h"
#include "ProjectManager.h"

#include <CHEngine/Input/InputSystem.h>
#include <Log/Log.h>
#include <Profiler.h>
#include <filesystem>

// Free function defined in SceneViewLayer_ImGuiFrame.cpp
void RunSceneViewImGuiFrame(SceneViewLayer& layer);


SceneViewLayer::SceneViewLayer(Ref<ProjectManager> proj_manager)
    : Layer("SceneView"), m_ProjectManager(proj_manager), m_ActiveIndex(0)
{
    UIActive::SetTheme(AppTheme::RetroOS);
    UIActive::SyncLayout();

    if (m_ProjectManager->HasProject())
    {
        OnProjectOpened();
        SceneViewLayerIO::TryRestoreSession(*this);
    }
    else
    {
        // No project yet — create one empty session so panels have a valid context
        auto ctx = new EditorWorldContext(m_Worlds.get());
        m_Worlds->PushBack(ctx);
        m_GizmoSystem.BindCommandStack(&ctx->CommandStack);
        SceneViewLayerCameraOps::ApplyOrbit(*this);
    }
}

void SceneViewLayer::OnProjectOpened()
{
    // Clear any sessions from a previous project and start fresh
    while (!m_Worlds->IsEmpty())
        m_Worlds->Erase(size_t(0));
    m_ActiveIndex = 0;

    auto ctx = new EditorWorldContext(m_Worlds.get());
    m_Worlds->PushBack(ctx);
    ctx->UpdateState(std::nullopt, true);

    auto proj = m_ProjectManager->Current();
    m_ContentBrowser.SetAssetsDirectory(proj->AssetsAbsPath());

    auto finish = [&] {
        SceneViewLayerCameraOps::ApplyOrbit(*this);
        m_GizmoSystem.BindCommandStack(&SceneViewLayerAccess::ActiveWorldCtx(*this)->CommandStack);
    };

    // 1. Try startup scene
    const std::string& startupRel = proj->GetStartupScene();
    if (!startupRel.empty())
    {
        const std::string absPath = proj->ResolveScenePath(startupRel);
        if (std::filesystem::exists(absPath))
        {
            SceneViewLayerIO::LoadSceneSilent(*this, absPath);
            finish();
            return;
        }
        CHE_CORE_WARN("SceneViewLayer: startup scene '{}' not found, scanning Scenes/", startupRel);
    }

    // 2. No startup scene (or missing file) — pick the first scene in Scenes/
    const std::filesystem::path scenesDir = proj->ScenesAbsPath();
    if (std::filesystem::is_directory(scenesDir))
    {
        for (const auto& entry : std::filesystem::directory_iterator(scenesDir))
        {
            if (entry.path().extension() == ".chscene")
            {
                SceneViewLayerIO::LoadSceneSilent(*this, entry.path().string());
                finish();
                return;
            }
        }
    }

    // 3. No scenes found — empty session is already ready
    finish();
}

void SceneViewLayer::OnUpdate(CHEngine::Timestep dt)
{
    CHE_PROFILE_FUNCTION();

    if (!m_ProjectManager->HasProject())
        return;

    if (m_Worlds->IsEmpty())
        return;

    auto active = SceneViewLayerAccess::ActiveWorldCtx(*this);
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
