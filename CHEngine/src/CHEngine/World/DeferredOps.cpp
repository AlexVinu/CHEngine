#include "chepch.h"
#include "DeferredOps.h"

#include <cstdint>
#include <unordered_set>

namespace CHEngine
{
    namespace
    {
        constexpr uint64_t kHookEventAdded = 0xA11Du;
        constexpr uint64_t kHookEventRemoved = 0xB22Eu;

        uint64_t MixHookKey(size_t component_hash, uint64_t event_kind, uint64_t function_key)
        {
            uint64_t result = static_cast<uint64_t>(component_hash);
            result ^= event_kind + 0x9e3779b97f4a7c15ULL + (result << 6u) + (result >> 2u);
            result ^= function_key + 0x9e3779b97f4a7c15ULL + (result << 6u) + (result >> 2u);
            return result;
        }
    }

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

    EntityHandle DeferredOps::ResolveTarget(Ref<Scene> scene, const DeferredEntityTarget& target, const std::unordered_map<DeferredEntityHandle, EntityHandle>& created_handles) const
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

    DeferredOps::HookToken DeferredOps::SubscribeOnComponentAdded(std::type_index component_type, ComponentAddedFn fn)
    {
        if (!fn)
            return 0;

        const uint64_t function_key = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(fn));
        const uint64_t dedup_key = MixHookKey(component_type.hash_code(), kHookEventAdded, function_key);
        const auto existing_it = m_DedupTokenByKey.find(dedup_key);
        if (existing_it != m_DedupTokenByKey.end())
            return existing_it->second;

        const HookToken token = m_NextHookToken++;
        ComponentHookBinding binding{};
        binding.Token = token;
        binding.ComponentType = component_type;
        binding.DedupKey = dedup_key;
        binding.AddedFn = fn;
        m_HookIndexByToken.emplace(token, m_ComponentHooks.size());
        m_DedupTokenByKey.emplace(dedup_key, token);
        m_ComponentHooks.emplace_back(binding);
        return token;
    }

    DeferredOps::HookToken DeferredOps::SubscribeOnComponentRemoved(std::type_index component_type, ComponentRemovedFn fn)
    {
        if (!fn)
            return 0;

        const uint64_t function_key = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(fn));
        const uint64_t dedup_key = MixHookKey(component_type.hash_code(), kHookEventRemoved, function_key);
        const auto existing_it = m_DedupTokenByKey.find(dedup_key);
        if (existing_it != m_DedupTokenByKey.end())
            return existing_it->second;

        const HookToken token = m_NextHookToken++;
        ComponentHookBinding binding{};
        binding.Token = token;
        binding.ComponentType = component_type;
        binding.DedupKey = dedup_key;
        binding.RemovedFn = fn;
        m_HookIndexByToken.emplace(token, m_ComponentHooks.size());
        m_DedupTokenByKey.emplace(dedup_key, token);
        m_ComponentHooks.emplace_back(binding);
        return token;
    }

    bool DeferredOps::Unsubscribe(HookToken token)
    {
        const auto token_it = m_HookIndexByToken.find(token);
        if (token_it == m_HookIndexByToken.end())
            return false;

        const size_t remove_index = token_it->second;
        if (remove_index >= m_ComponentHooks.size())
            return false;

        const ComponentHookBinding removed = m_ComponentHooks[remove_index];
        m_DedupTokenByKey.erase(removed.DedupKey);
        m_HookIndexByToken.erase(token_it);

        const size_t last_index = m_ComponentHooks.size() - 1;
        if (remove_index != last_index)
        {
            m_ComponentHooks[remove_index] = m_ComponentHooks[last_index];
            m_HookIndexByToken[m_ComponentHooks[remove_index].Token] = remove_index;
        }

        m_ComponentHooks.pop_back();
        return true;
    }

    void DeferredOps::DispatchOnComponentAdded(std::type_index component_type, World& world, EntityHandle entity_handle)
    {
        std::vector<ComponentAddedFn> callbacks;
        callbacks.reserve(m_ComponentHooks.size());

        for (const ComponentHookBinding& binding : m_ComponentHooks)
        {
            if (binding.ComponentType == component_type && binding.AddedFn)
                callbacks.emplace_back(binding.AddedFn);
        }

        for (ComponentAddedFn callback : callbacks)
            callback(world, entity_handle);
    }

    void DeferredOps::DispatchOnComponentRemoved(std::type_index component_type, World& world, EntityHandle entity_handle)
    {
        std::vector<ComponentRemovedFn> callbacks;
        callbacks.reserve(m_ComponentHooks.size());

        for (const ComponentHookBinding& binding : m_ComponentHooks)
        {
            if (binding.ComponentType == component_type && binding.RemovedFn)
                callbacks.emplace_back(binding.RemovedFn);
        }

        for (ComponentRemovedFn callback : callbacks)
            callback(world, entity_handle);
    }

    void DeferredOps::Flush(World& world, Ref<Scene> scene)
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
                add_command->Apply(world, scene, entity_handle);
                continue;
            }

            if (const auto* remove_command = std::get_if<RemoveComponentCommand>(&command_record))
            {
                const EntityHandle entity_handle = ResolveTarget(scene, remove_command->Target, created_handles);
                if (!scene->IsEntityHandleValid(entity_handle))
                    continue;
                if (pending_destroy_keys.contains(HandleToKey(entity_handle)))
                    continue;
                remove_command->Apply(world, scene, entity_handle);
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