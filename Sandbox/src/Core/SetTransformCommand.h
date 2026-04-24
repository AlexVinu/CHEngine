#pragma once

#include <CHEngine.h>
#include <CHEngine/Scene/Components.h>
#include <CHEngine/Scene/Entity.h>
#include <CHEngine/Scene/Scene.h>

#include <functional>
#include <vector>

namespace Sandbox {

class ICommand
{
public:
    virtual ~ICommand() = default;
    virtual void Execute() = 0;
    virtual void Undo() = 0;
    /// When false, Execute() runs but the command is not stored for Undo/Redo.
    virtual bool RecordsUndo() const { return true; }
};

class CommandStack
{
public:
    static constexpr int k_MaxSize = 50;

    void Push(CHEngine::Scope<ICommand> command)
    {
        if (!command)
            return;

        command->Execute();
        if (!command->RecordsUndo())
            return;

        if (static_cast<int>(m_UndoStack.size()) >= k_MaxSize)
            m_UndoStack.erase(m_UndoStack.begin());
        m_UndoStack.push_back(std::move(command));
        m_RedoStack.clear();
    }

    void Undo()
    {
        if (m_UndoStack.empty())
            return;

        CHEngine::Scope<ICommand> command = std::move(m_UndoStack.back());
        m_UndoStack.pop_back();
        command->Undo();
        m_RedoStack.push_back(std::move(command));
    }

    void Redo()
    {
        if (m_RedoStack.empty())
            return;

        CHEngine::Scope<ICommand> command = std::move(m_RedoStack.back());
        m_RedoStack.pop_back();
        command->Execute();
        m_UndoStack.push_back(std::move(command));
    }

    bool CanUndo() const { return !m_UndoStack.empty(); }
    bool CanRedo() const { return !m_RedoStack.empty(); }

    void Clear()
    {
        m_UndoStack.clear();
        m_RedoStack.clear();
    }

private:
    std::vector<CHEngine::Scope<ICommand>> m_UndoStack;
    std::vector<CHEngine::Scope<ICommand>> m_RedoStack;
};

class SetTransformCommand final : public ICommand
{
public:
    SetTransformCommand(CHEngine::Ref<CHEngine::Scene> scene,
                        CHEngine::EntityHandle entity_handle,
                        const CHEngine::Transform& before_transform,
                        const CHEngine::Transform& after_transform)
        : m_Scene(scene)
        , m_EntityHandle(entity_handle)
        , m_BeforeTransform(before_transform)
        , m_AfterTransform(after_transform)
    {
    }

    void Execute() override
    {
        ApplyTransform(m_AfterTransform);
    }

    void Undo() override
    {
        ApplyTransform(m_BeforeTransform);
    }

private:
    void ApplyTransform(const CHEngine::Transform& transform)
    {
        if (!m_Scene || !m_Scene->IsEntityHandleValid(m_EntityHandle))
            return;

        auto* entity = m_Scene->TryGetEntity(m_EntityHandle);
        if (!entity || !entity->HasComponent<CHEngine::TransformComponent>())
            return;

        entity->GetComponent<CHEngine::TransformComponent>().ObjectTransform = transform;
    }

private:
    CHEngine::Ref<CHEngine::Scene> m_Scene = nullptr;
    CHEngine::EntityHandle m_EntityHandle{};
    CHEngine::Transform m_BeforeTransform{};
    CHEngine::Transform m_AfterTransform{};
};

class CallbackCommand final : public ICommand
{
public:
    CallbackCommand(std::function<void()> execute_fn, std::function<void()> undo_fn, bool records_undo = false)
        : m_ExecuteFn(std::move(execute_fn))
        , m_UndoFn(std::move(undo_fn))
        , m_RecordsUndo(records_undo)
    {
    }

    void Execute() override
    {
        if (m_ExecuteFn)
            m_ExecuteFn();
    }

    void Undo() override
    {
        if (m_UndoFn)
            m_UndoFn();
    }

    bool RecordsUndo() const override { return m_RecordsUndo; }

private:
    std::function<void()> m_ExecuteFn;
    std::function<void()> m_UndoFn;
    bool m_RecordsUndo = false;
};

class SetVisibilityCommand final : public ICommand
{
public:
    SetVisibilityCommand(CHEngine::Ref<CHEngine::Scene> scene,
                         const CHEngine::UUID& object_id,
                         bool before_visible,
                         bool after_visible)
        : m_Scene(scene)
        , m_ObjectId(object_id)
        , m_BeforeVisible(before_visible)
        , m_AfterVisible(after_visible)
    {
    }

    void Execute() override { ApplyVisible(m_AfterVisible); }
    void Undo() override { ApplyVisible(m_BeforeVisible); }

private:
    void ApplyVisible(bool visible)
    {
        if (!m_Scene)
            return;
        const CHEngine::EntityHandle handle = m_Scene->TryGetEntityHandleByUUID(m_ObjectId);
        auto* entity = m_Scene->TryGetEntity(handle);
        if (!entity || !entity->HasComponent<CHEngine::VisibilityComponent>())
            return;
        entity->GetComponent<CHEngine::VisibilityComponent>().Visible = visible;
    }

private:
    CHEngine::Ref<CHEngine::Scene> m_Scene = nullptr;
    CHEngine::UUID m_ObjectId{};
    bool m_BeforeVisible = false;
    bool m_AfterVisible = false;
};

} // namespace Sandbox
