#pragma once

#include <vector>
#include <CHEngine.h>

// ============================================================================
//  UndoStack — простая история действий (до 50 шагов)
//
//  Использование:
//    // Перед изменением — сохранить состояние:
//    m_UndoStack.PushTransform(&m_Scene, obj->ID, obj->ObjectTransform);
//
//    // Отмена:
//    m_UndoStack.Undo();
// ============================================================================

// ----------------------------------------------------------------------------
//  Базовый command
// ----------------------------------------------------------------------------
struct UndoCommand
{
    virtual ~UndoCommand() = default;
    virtual void Undo() = 0;
};

// ----------------------------------------------------------------------------
//  Отмена трансформа (гизмо, слайдеры, Reset Transform)
// ----------------------------------------------------------------------------
struct TransformCommand : UndoCommand
{
    CHEngine::Scene*     scene;
    CHEngine::TagComponentIDType objectID;
    CHEngine::Transform  before;

    void Undo() override
    {
        auto handle = scene->TryGetEntityHandleByID(objectID);
        if (auto* transform = scene->TryGetComponent<CHEngine::TransformComponent>(handle))
            transform->ObjectTransform = before;
    }
};

// ----------------------------------------------------------------------------
//  Отмена видимости
// ----------------------------------------------------------------------------
struct VisibilityCommand : UndoCommand
{
    CHEngine::Scene* scene;
    CHEngine::TagComponentIDType objectID;
    bool             before;

    void Undo() override
    {
        auto handle = scene->TryGetEntityHandleByID(objectID);
        if (auto* visibility = scene->TryGetComponent<CHEngine::VisibilityComponent>(handle))
            visibility->Visible = before;
    }
};

// ----------------------------------------------------------------------------
//  Отмена импорта (удаляет объект из сцены)
// ----------------------------------------------------------------------------
struct ImportCommand : UndoCommand
{
    CHEngine::Scene* scene;
    CHEngine::TagComponentIDType objectID;
    CHEngine::TagComponentIDType* selectedID;   // указатель на m_SelectedObjectID

    void Undo() override
    {
        if (*selectedID == objectID)
            *selectedID = 0;
        scene->RemoveObject(objectID);
    }
};

// ----------------------------------------------------------------------------
//  Стек
// ----------------------------------------------------------------------------
class UndoStack
{
public:
    static constexpr int k_MaxSize = 50;

    void Push(CHEngine::Scope<UndoCommand> cmd)
    {
        if ((int)m_Stack.size() >= k_MaxSize)
            m_Stack.erase(m_Stack.begin());
        m_Stack.push_back(std::move(cmd));
    }

    // Хелперы — чтобы не писать make_unique каждый раз
    void PushTransform(CHEngine::Scene* scene, CHEngine::TagComponentIDType id,
                       const CHEngine::Transform& before)
    {
        auto cmd    = std::make_unique<TransformCommand>();
        cmd->scene  = scene;
        cmd->objectID = id;
        cmd->before = before;
        Push(std::move(cmd));
    }

    void PushVisibility(CHEngine::Scene* scene, CHEngine::TagComponentIDType id, bool before)
    {
        auto cmd      = std::make_unique<VisibilityCommand>();
        cmd->scene    = scene;
        cmd->objectID = id;
        cmd->before   = before;
        Push(std::move(cmd));
    }

    void PushImport(CHEngine::Scene* scene, CHEngine::TagComponentIDType id, CHEngine::TagComponentIDType* selectedID)
    {
        auto cmd           = std::make_unique<ImportCommand>();
        cmd->scene         = scene;
        cmd->objectID      = id;
        cmd->selectedID    = selectedID;
        Push(std::move(cmd));
    }

    void Undo()
    {
        if (m_Stack.empty()) return;
        m_Stack.back()->Undo();
        m_Stack.pop_back();
    }

    bool CanUndo() const { return !m_Stack.empty(); }
    void Clear()         { m_Stack.clear(); }

private:
    std::vector<CHEngine::Scope<UndoCommand>> m_Stack;
};
