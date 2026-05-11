#include "EditorWorldContext.h"

#include <CHEngine/World/World.h>
#include <Log/Log.h>

#include <algorithm>
#include <filesystem>

EditorWorldContext::EditorWorldContext()
    : SceneSession()
{
}

void EditorWorldContext::Update(CHEngine::Timestep dt)
{
    CHE_CORE_ASSERT(EditorScene, "EditorWorldContext must have EditorScene");

    switch (SessionState)
    {
    case SceneSession::State::Edit:
        if (!IsActive) return;
        RuntimeWorld->SetState(CHEngine::WorldState::Presenting);
        RuntimeWorld->SetActiveCamera(ViewportCamera.get());
        RuntimeWorld->Update(dt);
        break;

    case SceneSession::State::Play:
        if (IsActive)
        {
            RuntimeWorld->SetState(CHEngine::WorldState::Simulating);
            RuntimeWorld->SetActiveCamera(nullptr);
        }
        else
            RuntimeWorld->SetState(CHEngine::WorldState::SimulatingWithoutPresenting);

        RuntimeWorld->Update(dt);
        break;

    case SceneSession::State::Pause:
        if (!IsActive) return;
        RuntimeWorld->SetState(CHEngine::WorldState::Presenting);
        RuntimeWorld->SetActiveCamera(ViewportCamera.get());
        RuntimeWorld->Update(dt);
        break;
    }
}

void EditorWorldContext::ActivateActiveScene()
{
    CHE_ASSERT(ActiveScene, "THERE ARE NO ACTIVE SCENE");
    CHE_ASSERT(EditorScene, "THERE ARE NO EDITOR SCENE");
    CHE_ASSERT(RuntimeWorld, "THERE ARE NO RuntimeWorld");

    *ActiveScene = *EditorScene;
    RuntimeWorld->SetScene(ActiveScene);
}

void EditorWorldContext::ActivateEditorScene()
{
    CHE_ASSERT(ActiveScene, "THERE ARE NO ACTIVE SCENE");
    CHE_ASSERT(EditorScene, "THERE ARE NO EDITOR SCENE");
    CHE_ASSERT(RuntimeWorld, "THERE ARE NO RuntimeWorld");

    *ActiveScene = *EditorScene;
    RuntimeWorld->SetScene(EditorScene);
}

std::string EditorWorldContext::DisplayName() const
{
    if (SceneRelPath.empty())
        return "Untitled";
    std::string name = std::filesystem::path(SceneRelPath).stem().string();
    std::replace(name.begin(), name.end(), '_', ' ');
    return name;
}
