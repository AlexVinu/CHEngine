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
    if (m_EditorState != EditorState::Edit)
        return;

    // 1. Сохранить снапшот сцены в память
    CHEngine::SceneSerializer serializer{};
    m_SceneSnapshot = serializer.SerializeToJson(&m_Scene);
    m_HasSnapshot   = true;

    // 2. Очистить undo (в Play режиме undo недоступен)
    m_UndoStack.Clear();

    // 3. Пересобрать физику и включить симуляцию
    m_World.GetEvents().Publish<CHEngine::RebuildPhysicsWorldEvent>(CHEngine::SystemPhase::Simulation);
    FlushSimulationEvents();
    m_World.SetSimulating(true);

    m_EditorState = EditorState::Play;
}

void SceneViewLayer::EnterPauseMode()
{
    if (m_EditorState != EditorState::Play)
        return;

    // Остановить симуляцию, но рендер (m_Active) продолжается
    m_World.SetSimulating(false);

    m_EditorState = EditorState::Pause;
}

void SceneViewLayer::ResumeFromPause()
{
    if (m_EditorState != EditorState::Pause)
        return;

    m_World.SetSimulating(true);

    m_EditorState = EditorState::Play;
}

void SceneViewLayer::StopPlayMode()
{
    if (m_EditorState == EditorState::Edit)
        return;

    // 1. Остановить симуляцию и физику
    m_World.SetSimulating(false);
    m_World.GetEvents().Publish<CHEngine::DestroyPhysicsWorldEvent>(CHEngine::SystemPhase::Simulation);
    FlushSimulationEvents();

    // 2. Восстановить сцену из снапшота
    if (m_HasSnapshot)
    {
        auto* resources = CHEngine::Application::Get().GetRenderResources();
        if (resources)
        {
            CHEngine::SceneSerializer serializer{};
            serializer.DeserializeFromJson(&m_Scene, m_SceneSnapshot, *resources, &m_World);
            m_World.GetEvents().Publish<CHEngine::RebuildPhysicsWorldEvent>(CHEngine::SystemPhase::Simulation);
            FlushSimulationEvents();
        }
        m_HasSnapshot  = false;
        m_SceneSnapshot = {};
    }

    // 3. Сбросить выделение (оно могло указывать на удалённую сущность)
    m_SelectedObjectID = boost::uuids::nil_uuid();

    m_EditorState = EditorState::Edit;
}

void SceneViewLayer::StepOneFrame()
{
    if (m_EditorState != EditorState::Pause)
        return;

    // Выполнить один кадр симуляции с фиксированным dt
    m_World.SetSimulating(true);
    m_World.Update(CHEngine::Timestep(m_StepDt));
    m_World.SetSimulating(false);
}
