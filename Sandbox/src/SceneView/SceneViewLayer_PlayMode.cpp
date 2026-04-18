#include "SceneViewLayer_Play.h"

#include "SceneViewLayer.h"
#include "SceneViewLayerAccess.h"

#include <CHEngine/Render/RenderResourceManager.h>
#include <CHEngine/Scene/SceneSerializer.h>
#include <CHEngine/World/ISystem.h>
#include <CHEngine/World/WorldEvents.h>

void SceneViewLayerPlay::EnterPlayMode(SceneViewLayer& layer)
{
    EditorWorldContext& activeSession = SceneViewLayerAccess::Active(layer);
    if (activeSession.SessionState != SceneSession::State::Edit)
        return;

    CHE_CORE_ASSERT(activeSession.EditorScene, "SceneViewLayer: EditorScene must exist");
    CHEngine::Scene& editorScene = *activeSession.EditorScene;
    if (!activeSession.RuntimeWorld)
        activeSession.RuntimeWorld = CHEngine::MakeScope<CHEngine::World>(activeSession.EditorScene.get());
    CHEngine::World& runtimeWorld = *activeSession.RuntimeWorld;

    CHEngine::SceneSerializer serializer{};
    activeSession.m_SceneSnapshot = serializer.SerializeToJson(&editorScene);
    activeSession.m_HasSceneSnapshot = true;

    auto* resources = CHEngine::Application::Get().GetRenderResources();
    if (resources)
    {
        CHEngine::Scope<CHEngine::Scene> runtimeScene = serializer.DeserializeFromJson(activeSession.m_SceneSnapshot, *resources);
        if (runtimeScene)
            activeSession.ActiveScene = CHEngine::CreateRef<CHEngine::Scene>(std::move(*runtimeScene));
    }
    if (!activeSession.ActiveScene)
        activeSession.ActiveScene = activeSession.EditorScene;

    activeSession.m_CommandStack.Clear();

    CHE_CORE_ASSERT(activeSession.ActiveScene, "SceneViewLayer: ActiveScene must exist");
    runtimeWorld.SetScene(activeSession.ActiveScene.get());
    runtimeWorld.GetEvents().Publish<CHEngine::RebuildPhysicsWorldEvent>(CHEngine::SystemPhase::Simulation);
    runtimeWorld.Update(CHEngine::Timestep(0.0f));
    runtimeWorld.SetSimulating(true);

    activeSession.SessionState = SceneSession::State::Play;
}

void SceneViewLayerPlay::EnterPauseMode(SceneViewLayer& layer)
{
    EditorWorldContext& activeSession = SceneViewLayerAccess::Active(layer);
    if (activeSession.SessionState != SceneSession::State::Play)
        return;

    CHE_CORE_ASSERT(activeSession.RuntimeWorld, "SceneViewLayer: RuntimeWorld must exist");
    CHEngine::World& runtimeWorld = *activeSession.RuntimeWorld;

    runtimeWorld.SetSimulating(false);

    activeSession.SessionState = SceneSession::State::Pause;
}

void SceneViewLayerPlay::ResumeFromPause(SceneViewLayer& layer)
{
    EditorWorldContext& activeSession = SceneViewLayerAccess::Active(layer);
    if (activeSession.SessionState != SceneSession::State::Pause)
        return;

    CHE_CORE_ASSERT(activeSession.RuntimeWorld, "SceneViewLayer: RuntimeWorld must exist");
    CHEngine::World& runtimeWorld = *activeSession.RuntimeWorld;
    runtimeWorld.SetSimulating(true);

    activeSession.SessionState = SceneSession::State::Play;
}

void SceneViewLayerPlay::StopPlayMode(SceneViewLayer& layer)
{
    EditorWorldContext& activeSession = SceneViewLayerAccess::Active(layer);
    if (activeSession.SessionState == SceneSession::State::Edit)
        return;

    CHE_CORE_ASSERT(activeSession.RuntimeWorld, "SceneViewLayer: RuntimeWorld must exist");
    CHEngine::World& runtimeWorld = *activeSession.RuntimeWorld;

    runtimeWorld.SetSimulating(false);
    runtimeWorld.GetEvents().Publish<CHEngine::DestroyPhysicsWorldEvent>(CHEngine::SystemPhase::Simulation);
    runtimeWorld.Update(CHEngine::Timestep(0.0f));

    if (activeSession.m_HasSceneSnapshot)
    {
        auto* resources = CHEngine::Application::Get().GetRenderResources();
        if (resources)
        {
            CHEngine::SceneSerializer serializer{};
            CHEngine::Scope<CHEngine::Scene> restoredScene
                = serializer.DeserializeFromJson(activeSession.m_SceneSnapshot, *resources);
            if (restoredScene)
            {
                activeSession.EditorScene = CHEngine::CreateRef<CHEngine::Scene>(std::move(*restoredScene));
            }
        }
        activeSession.m_HasSceneSnapshot = false;
        activeSession.m_SceneSnapshot = {};
    }

    activeSession.ActiveScene = activeSession.EditorScene;
    runtimeWorld.SetScene(activeSession.EditorScene.get());

    activeSession.SelectedEntity = {};

    activeSession.SessionState = SceneSession::State::Edit;
}

void SceneViewLayerPlay::StepOneFrame(SceneViewLayer& layer)
{
    EditorWorldContext& activeSession = SceneViewLayerAccess::Active(layer);
    if (activeSession.SessionState != SceneSession::State::Pause)
        return;

    CHE_CORE_ASSERT(activeSession.RuntimeWorld, "SceneViewLayer: RuntimeWorld must exist");
    CHEngine::World& runtimeWorld = *activeSession.RuntimeWorld;

    runtimeWorld.SetSimulating(true);
    runtimeWorld.Update(CHEngine::Timestep(activeSession.m_StepDt));
    runtimeWorld.SetSimulating(false);
}
