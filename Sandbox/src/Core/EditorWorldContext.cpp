#include "EditorWorldContext.h"

#include <CHEngine/World/World.h>
#include <Log/Log.h>

EditorWorldContext::EditorWorldContext()
    : SceneSession()
{
}

void EditorWorldContext::Update(CHEngine::Timestep dt)
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
        RuntimeWorld->SetState(CHEngine::WorldState::Presenting);
        RuntimeWorld->SetActiveCamera(ViewportCamera.get());
        RuntimeWorld->Update(dt);
        break;

    case SceneSession::State::Simulate:
        RuntimeWorld->SetState(CHEngine::WorldState::Simulating);
        RuntimeWorld->SetActiveCamera(ViewportCamera.get());
        RuntimeWorld->Update(dt);
        break;

    case SceneSession::State::Play:
        CHE_CORE_ASSERT(ActiveScene, "EditorWorldContext must have ActiveScene in Play");
        RuntimeWorld->SetState(CHEngine::WorldState::Simulating);
        RuntimeWorld->SetActiveCamera(nullptr);
        RuntimeWorld->Update(dt);
        break;

    case SceneSession::State::Pause:
        CHE_CORE_ASSERT(ActiveScene, "EditorWorldContext must have ActiveScene in Pause");
        RuntimeWorld->SetState(CHEngine::WorldState::Presenting);
        RuntimeWorld->SetActiveCamera(ViewportCamera.get());
        RuntimeWorld->Update(dt);
        break;
    }
}
