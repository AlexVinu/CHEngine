#include "EditorWorldContext.h"

#include <CHEngine/World/World.h>
#include <Log/Log.h>

EditorWorldContext::EditorWorldContext()
    : SceneSession()
{
}

void EditorWorldContext::UpdateSimulation(CHEngine::Timestep dt)
{
    if (!RuntimeWorld)
    {
        CHE_CORE_ASSERT(ActiveScene, "EditorWorldContext must have ActiveScene for RuntimeWorld");
        RuntimeWorld = CHEngine::MakeScope<CHEngine::World>(ActiveScene.get());
    }

    CHE_CORE_ASSERT(EditorScene, "EditorWorldContext must have EditorScene");

    switch (SessionState)
    {
    case SceneSession::State::Edit:
        RuntimeWorld->SetSimulating(false);
        RuntimeWorld->SetActiveCamera(ViewportCamera.get());
        RuntimeWorld->Update(dt);
        break;

    case SceneSession::State::Simulate:
        RuntimeWorld->SetSimulating(true);
        RuntimeWorld->SetActiveCamera(ViewportCamera.get());
        RuntimeWorld->Update(dt);
        break;

    case SceneSession::State::Play:
        CHE_CORE_ASSERT(ActiveScene, "EditorWorldContext must have ActiveScene in Play");
        RuntimeWorld->SetSimulating(true);
        RuntimeWorld->SetActiveCamera(nullptr);
        RuntimeWorld->Update(dt);
        break;

    case SceneSession::State::Pause:
        CHE_CORE_ASSERT(ActiveScene, "EditorWorldContext must have ActiveScene in Pause");
        RuntimeWorld->SetSimulating(false);
        RuntimeWorld->SetActiveCamera(ViewportCamera.get());
        RuntimeWorld->Update(dt);
        break;
    }
}
