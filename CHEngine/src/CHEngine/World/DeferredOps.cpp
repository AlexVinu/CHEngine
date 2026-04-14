#include "chepch.h"
#include "DeferredOps.h"

#include <unordered_set>

namespace CHEngine
{
    DeferredEntityHandle DeferredOps::CreateEntity(const std::string& name)
    {
        CreateEntityCommand command{};
        command.DeferredEntity = m_NextDeferredEntity++;
        command.Name = name;
        m_Commands.emplace_back(std::move(command));
        return command.DeferredEntity;
    }

    DeferredEntityHandle DeferredOps::CreateEntityWithUUID(const std::string& name, const UUID& uuid)
    {
        CreateEntityCommand command{};
        command.DeferredEntity = m_NextDeferredEntity++;
        command.Name = name;
        command.ExplicitUUID = uuid;
        m_Commands.emplace_back(std::move(command));
        return command.DeferredEntity;
    }

    void DeferredOps::DestroyEntity(EntityHandle entity_handle)
    {
        DestroyEntityCommand command{};
        command.Target = DeferredEntityTarget::FromEntityHandle(entity_handle);
        m_Commands.emplace_back(std::move(command));
    }

    void DeferredOps::DestroyEntity(DeferredEntityHandle deferred_entity_handle)
    {
        DestroyEntityCommand command{};
        command.Target = DeferredEntityTarget::FromDeferredHandle(deferred_entity_handle);
        m_Commands.emplace_back(std::move(command));
    }

    void DeferredOps::DestroyEntityByUUID(const UUID& uuid)
    {
        DestroyEntityCommand command{};
        command.Target = DeferredEntityTarget::FromUUID(uuid);
        m_Commands.emplace_back(std::move(command));
    }

    EntityHandle DeferredOps::ResolveTarget(Scene* scene, const DeferredEntityTarget& target, const std::unordered_map<DeferredEntityHandle, EntityHandle>& created_handles) const
    {
        if (!scene)
            return {};

        switch (target.TargetType)
        {
        case DeferredEntityTarget::Type::EntityHandle:
            return scene->IsEntityHandleValid(target.Entity) ? target.Entity : EntityHandle{};
        case DeferredEntityTarget::Type::DeferredEntityHandle:
        {
            const auto it = created_handles.find(target.DeferredEntity);
            if (it == created_handles.end())
                return {};
            return scene->IsEntityHandleValid(it->second) ? it->second : EntityHandle{};
        }
        case DeferredEntityTarget::Type::UUID:
        {
            const EntityHandle entity_handle = scene->TryGetEntityHandleByUUID(target.EntityUUID);
            return scene->IsEntityHandleValid(entity_handle) ? entity_handle : EntityHandle{};
        }
        default:
            break;
        }

        return {};
    }

    uint64_t DeferredOps::HandleToKey(EntityHandle entity_handle)
    {
        return (static_cast<uint64_t>(entity_handle.generation) << 32u) | static_cast<uint64_t>(entity_handle.index);
    }

    void DeferredOps::Flush(Scene* scene)
    {
        if (!scene)
        {
            Clear();
            return;
        }

        std::unordered_map<DeferredEntityHandle, EntityHandle> created_handles;
        created_handles.reserve(m_Commands.size());

        for (const DeferredCommandRecord& command_record : m_Commands)
        {
            const auto* create_command = std::get_if<CreateEntityCommand>(&command_record);
            if (!create_command)
                continue;

            EntityHandle created_entity{};
            if (create_command->ExplicitUUID.has_value())
                created_entity = scene->CreateEntity(create_command->Name, create_command->ExplicitUUID.value());
            else
                created_entity = scene->CreateEntity(create_command->Name);

            created_handles.emplace(create_command->DeferredEntity, created_entity);
        }

        std::unordered_set<uint64_t> pending_destroy_keys;
        pending_destroy_keys.reserve(m_Commands.size());
        std::vector<EntityHandle> destroy_order;
        destroy_order.reserve(m_Commands.size());

        for (const DeferredCommandRecord& command_record : m_Commands)
        {
            const auto* destroy_command = std::get_if<DestroyEntityCommand>(&command_record);
            if (!destroy_command)
                continue;

            const EntityHandle entity_handle = ResolveTarget(scene, destroy_command->Target, created_handles);
            if (!scene->IsEntityHandleValid(entity_handle))
                continue;

            const uint64_t handle_key = HandleToKey(entity_handle);
            if (!pending_destroy_keys.insert(handle_key).second)
                continue;

            destroy_order.push_back(entity_handle);
        }

        for (const DeferredCommandRecord& command_record : m_Commands)
        {
            if (const auto* add_command = std::get_if<AddComponentCommand>(&command_record))
            {
                const EntityHandle entity_handle = ResolveTarget(scene, add_command->Target, created_handles);
                if (!scene->IsEntityHandleValid(entity_handle))
                    continue;
                if (pending_destroy_keys.contains(HandleToKey(entity_handle)))
                    continue;
                add_command->Apply(scene, entity_handle);
                continue;
            }

            if (const auto* remove_command = std::get_if<RemoveComponentCommand>(&command_record))
            {
                const EntityHandle entity_handle = ResolveTarget(scene, remove_command->Target, created_handles);
                if (!scene->IsEntityHandleValid(entity_handle))
                    continue;
                if (pending_destroy_keys.contains(HandleToKey(entity_handle)))
                    continue;
                remove_command->Apply(scene, entity_handle);
                continue;
            }

            if (const auto* custom_command = std::get_if<CustomCommand>(&command_record))
            {
                if (custom_command->Callback)
                    custom_command->Callback(scene);
            }
        }

        for (const EntityHandle entity_handle : destroy_order)
        {
            if (scene->IsEntityHandleValid(entity_handle))
                scene->DestroyEntity(entity_handle);
        }

        Clear();
    }

    void DeferredOps::Clear()
    {
        m_Commands.clear();
    }
}