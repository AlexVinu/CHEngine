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

    activeSession.CommandStack.Clear();

    activeSession.ActivateActiveScene();
    activeSession.SessionState = SceneSession::State::Play;
}

void SceneViewLayerPlay::EnterPauseMode(SceneViewLayer& layer)
{
    EditorWorldContext& activeSession = SceneViewLayerAccess::Active(layer);
    if (activeSession.SessionState != SceneSession::State::Play)
        return;

    activeSession.SessionState = SceneSession::State::Pause;
}

void SceneViewLayerPlay::ResumeFromPause(SceneViewLayer& layer)
{
    EditorWorldContext& activeSession = SceneViewLayerAccess::Active(layer);
    if (activeSession.SessionState != SceneSession::State::Pause)
        return;

    activeSession.SessionState = SceneSession::State::Play;
}

void SceneViewLayerPlay::StopPlayMode(SceneViewLayer& layer)
{
    EditorWorldContext& activeSession = SceneViewLayerAccess::Active(layer);
    if (activeSession.SessionState == SceneSession::State::Edit)
        return;

    activeSession.ActivateEditorScene();
    activeSession.SelectedEntity = {};
    activeSession.SessionState = SceneSession::State::Edit;
}

//void SceneViewLayerPlay::StepOneFrame(SceneViewLayer& layer)
//{
//    EditorWorldContext& activeSession = SceneViewLayerAccess::Active(layer);
//    if (activeSession.SessionState != SceneSession::State::Pause)
//        return;
//
//    CHE_CORE_ASSERT(activeSession.RuntimeWorld, "SceneViewLayer: RuntimeWorld must exist");
//    CHEngine::World* runtime_world = activeSession.RuntimeWorld.get();
//    if (!runtime_world)
//        return;
//
//    runtime_world->SetState(CHEngine::WorldState::Simulating);
//    runtime_world->Update(CHEngine::Timestep(activeSession.StepDt));
//    runtime_world->SetState(CHEngine::WorldState::Presenting);
//}
