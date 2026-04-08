#include "SceneViewLayer.h"

#include <CHEngine/Scene/SceneSerializer.h>
#include <CHEngine/Render/RenderResourceManager.h>

// ============================================================================
//  Play / Pause / Stop logic
// ============================================================================

void SceneViewLayer::EnterPlayMode()
{
    if (m_EditorState != EditorState::Edit)
        return;

    // 1. Сохранить снапшот сцены в память
    CHEngine::SceneSerializer serializer(&m_Scene, &m_World);
    m_SceneSnapshot = serializer.SerializeToJson();
    m_HasSnapshot   = true;

    // 2. Очистить undo (в Play режиме undo недоступен)
    m_UndoStack.Clear();

    // 3. Пересобрать физику и включить симуляцию
    m_World.RebuildPhysicsRuntime();
    m_World.setSimulating(true);

    m_EditorState = EditorState::Play;
}

void SceneViewLayer::EnterPauseMode()
{
    if (m_EditorState != EditorState::Play)
        return;

    // Остановить симуляцию, но рендер (m_Active) продолжается
    m_World.setSimulating(false);

    m_EditorState = EditorState::Pause;
}

void SceneViewLayer::ResumeFromPause()
{
    if (m_EditorState != EditorState::Pause)
        return;

    m_World.setSimulating(true);

    m_EditorState = EditorState::Play;
}

void SceneViewLayer::StopPlayMode()
{
    if (m_EditorState == EditorState::Edit)
        return;

    // 1. Остановить симуляцию и физику
    m_World.setSimulating(false);
    m_World.ClearPhysicsRuntime();

    // 2. Восстановить сцену из снапшота
    if (m_HasSnapshot)
    {
        auto* resources = CHEngine::Application::Get().GetRenderResources();
        if (resources)
        {
            CHEngine::SceneSerializer serializer(&m_Scene, &m_World);
            serializer.DeserializeFromJson(m_SceneSnapshot, *resources);
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
    m_World.setSimulating(true);
    m_World.update(CHEngine::Timestep(m_StepDt));
    m_World.setSimulating(false);
}
