#include "SceneViewLayer_Play.h"

#include "SceneViewLayer.h"
#include "SceneViewLayerAccess.h"

#include <CHEngine/World/ISystem.h>
#include <CHEngine/World/World.h>

void SceneViewLayerPlay::EnterPlayMode(SceneViewLayer& layer)
{
    EditorWorldContext& activeSession = SceneViewLayerAccess::Active(layer);
    if (activeSession.SessionState != SceneSession::State::Edit)
        return;

    CHE_CORE_ASSERT(activeSession.EditorScene, "SceneViewLayer: EditorScene must exist");
    auto editor_scene_ref = activeSession.EditorScene;
    if (!editor_scene_ref)
        return;

    if (!activeSession.RuntimeWorld)
        activeSession.RuntimeWorld = CHEngine::MakeScope<CHEngine::World>(activeSession.EditorScene.get());

    CHEngine::World* runtime_world = activeSession.RuntimeWorld.get();
    if (!runtime_world)
        return;

    activeSession.ActiveScene = CHEngine::CreateRef<CHEngine::Scene>(*editor_scene_ref);
    if (!activeSession.ActiveScene)
        return;

    activeSession.m_CommandStack.Clear();

    CHE_CORE_ASSERT(activeSession.ActiveScene, "SceneViewLayer: ActiveScene must exist");
    runtime_world->SetScene(activeSession.ActiveScene.get());
    runtime_world->SetState(CHEngine::WorldState::Simulating);

    activeSession.SessionState = SceneSession::State::Play;
}

void SceneViewLayerPlay::EnterPauseMode(SceneViewLayer& layer)
{
    EditorWorldContext& activeSession = SceneViewLayerAccess::Active(layer);
    if (activeSession.SessionState != SceneSession::State::Play)
        return;

    CHE_CORE_ASSERT(activeSession.RuntimeWorld, "SceneViewLayer: RuntimeWorld must exist");
    CHEngine::World* runtime_world = activeSession.RuntimeWorld.get();
    if (!runtime_world)
        return;

    runtime_world->SetState(CHEngine::WorldState::Presenting);

    activeSession.SessionState = SceneSession::State::Pause;
}

void SceneViewLayerPlay::ResumeFromPause(SceneViewLayer& layer)
{
    EditorWorldContext& activeSession = SceneViewLayerAccess::Active(layer);
    if (activeSession.SessionState != SceneSession::State::Pause)
        return;

    CHE_CORE_ASSERT(activeSession.RuntimeWorld, "SceneViewLayer: RuntimeWorld must exist");
    CHEngine::World* runtime_world = activeSession.RuntimeWorld.get();
    if (!runtime_world)
        return;
    runtime_world->SetState(CHEngine::WorldState::Simulating);

    activeSession.SessionState = SceneSession::State::Play;
}

void SceneViewLayerPlay::StopPlayMode(SceneViewLayer& layer)
{
    EditorWorldContext& activeSession = SceneViewLayerAccess::Active(layer);
    if (activeSession.SessionState == SceneSession::State::Edit)
        return;

    CHE_CORE_ASSERT(activeSession.RuntimeWorld, "SceneViewLayer: RuntimeWorld must exist");
    CHEngine::World* runtime_world = activeSession.RuntimeWorld.get();
    if (!runtime_world)
        return;

    runtime_world->SetState(CHEngine::WorldState::Presenting);

    activeSession.ActiveScene = activeSession.EditorScene;
    runtime_world->SetScene(activeSession.EditorScene.get());

    activeSession.SelectedEntity = {};

    activeSession.SessionState = SceneSession::State::Edit;
}

void SceneViewLayerPlay::StepOneFrame(SceneViewLayer& layer)
{
    EditorWorldContext& activeSession = SceneViewLayerAccess::Active(layer);
    if (activeSession.SessionState != SceneSession::State::Pause)
        return;

    CHE_CORE_ASSERT(activeSession.RuntimeWorld, "SceneViewLayer: RuntimeWorld must exist");
    CHEngine::World* runtime_world = activeSession.RuntimeWorld.get();
    if (!runtime_world)
        return;

    runtime_world->SetState(CHEngine::WorldState::Simulating);
    runtime_world->Update(CHEngine::Timestep(activeSession.m_StepDt));
    runtime_world->SetState(CHEngine::WorldState::Presenting);
}
