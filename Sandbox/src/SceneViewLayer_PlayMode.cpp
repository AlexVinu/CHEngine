#include "SceneViewLayer.h"

#include <CHEngine/Render/RenderResourceManager.h>
#include <CHEngine/Scene/SceneSerializer.h>
#include <CHEngine/World/ISystem.h>
#include <CHEngine/World/WorldEvents.h>

// ============================================================================
//  Play / Pause / Stop logic
// ============================================================================

void SceneViewLayer::EnterPlayMode()
{
    SceneSession& activeSession = GetActiveSceneSession();
    if (activeSession.SessionState != SceneSession::State::Edit)
        return;

    CHE_CORE_ASSERT(activeSession.EditorScene, "SceneViewLayer: EditorScene must exist");
    CHEngine::Scene& editorScene = *activeSession.EditorScene;
    if (!activeSession.RuntimeWorld)
        activeSession.RuntimeWorld = CHEngine::MakeScope<CHEngine::World>(activeSession.EditorScene.get());
    CHEngine::World& runtimeWorld = *activeSession.RuntimeWorld;

    // 1. Сохранить снапшот сцены в память
    CHEngine::SceneSerializer serializer{};
    m_SceneSnapshot = serializer.SerializeToJson(&editorScene);
    m_HasSnapshot   = true;

    auto* resources = CHEngine::Application::Get().GetRenderResources();
    if (resources)
    {
        CHEngine::Scope<CHEngine::Scene> runtimeScene = serializer.DeserializeFromJson(m_SceneSnapshot, *resources);
        if (runtimeScene)
            GetActiveSceneSession().ActiveScene = CHEngine::CreateRef<CHEngine::Scene>(std::move(*runtimeScene));
    }

    // 2. Очистить undo (в Play режиме undo недоступен)
    m_UndoStack.Clear();

    // 3. Пересобрать физику и включить симуляцию
    CHE_CORE_ASSERT(GetActiveSceneSession().ActiveScene, "SceneViewLayer: ActiveScene must exist");
    runtimeWorld.SetScene(GetActiveSceneSession().ActiveScene.get());
    runtimeWorld.GetEvents().Publish<CHEngine::RebuildPhysicsWorldEvent>(CHEngine::SystemPhase::Simulation);
    runtimeWorld.Update(CHEngine::Timestep(0.0f));
    runtimeWorld.SetSimulating(true);

    GetActiveSceneSession().SessionState = SceneSession::State::Play;
}

void SceneViewLayer::EnterPauseMode()
{
    SceneSession& activeSession = GetActiveSceneSession();
    if (activeSession.SessionState != SceneSession::State::Play)
        return;

    CHE_CORE_ASSERT(activeSession.RuntimeWorld, "SceneViewLayer: RuntimeWorld must exist");
    CHEngine::World& runtimeWorld = *activeSession.RuntimeWorld;

    // Остановить симуляцию, но рендер (m_Active) продолжается
    runtimeWorld.SetSimulating(false);

    activeSession.SessionState = SceneSession::State::Pause;
}

void SceneViewLayer::ResumeFromPause()
{
    SceneSession& activeSession = GetActiveSceneSession();
    if (activeSession.SessionState != SceneSession::State::Pause)
        return;

    CHE_CORE_ASSERT(activeSession.RuntimeWorld, "SceneViewLayer: RuntimeWorld must exist");
    CHEngine::World& runtimeWorld = *activeSession.RuntimeWorld;
    runtimeWorld.SetSimulating(true);

    activeSession.SessionState = SceneSession::State::Play;
}

void SceneViewLayer::StopPlayMode()
{
    SceneSession& activeSession = GetActiveSceneSession();
    if (activeSession.SessionState == SceneSession::State::Edit)
        return;

    CHE_CORE_ASSERT(activeSession.RuntimeWorld, "SceneViewLayer: RuntimeWorld must exist");
    CHEngine::World& runtimeWorld = *activeSession.RuntimeWorld;

    // 1. Остановить симуляцию и физику
    runtimeWorld.SetSimulating(false);
    runtimeWorld.GetEvents().Publish<CHEngine::DestroyPhysicsWorldEvent>(CHEngine::SystemPhase::Simulation);
    runtimeWorld.Update(CHEngine::Timestep(0.0f));

    // 2. Восстановить сцену из снапшота
    if (m_HasSnapshot)
    {
        auto* resources = CHEngine::Application::Get().GetRenderResources();
        if (resources)
        {
            CHEngine::SceneSerializer serializer{};
            CHEngine::Scope<CHEngine::Scene> restoredScene = serializer.DeserializeFromJson(m_SceneSnapshot, *resources);
            if (restoredScene) {
                activeSession.EditorScene = CHEngine::CreateRef<CHEngine::Scene>(std::move(*restoredScene));
            }
        }
        m_HasSnapshot  = false;
        m_SceneSnapshot = {};
    }

    activeSession.ActiveScene = activeSession.EditorScene;
    runtimeWorld.SetScene(activeSession.EditorScene.get());

    // 3. Сбросить выделение (оно могло указывать на удалённую сущность)
    activeSession.SelectedEntity = {};

    activeSession.SessionState = SceneSession::State::Edit;
}

void SceneViewLayer::StepOneFrame()
{
    SceneSession& activeSession = GetActiveSceneSession();
    if (activeSession.SessionState != SceneSession::State::Pause)
        return;

    CHE_CORE_ASSERT(activeSession.RuntimeWorld, "SceneViewLayer: RuntimeWorld must exist");
    CHEngine::World& runtimeWorld = *activeSession.RuntimeWorld;

    // Выполнить один кадр симуляции с фиксированным dt
    runtimeWorld.SetSimulating(true);
    runtimeWorld.Update(CHEngine::Timestep(m_StepDt));
    runtimeWorld.SetSimulating(false);
}
