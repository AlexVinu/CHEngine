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

        constexpr uint64_t MAGIC_NUMBER = 0x9e3779b97f4a7c15ULL;

        uint64_t MixHookKey(size_t component_hash, uint64_t event_kind, uint64_t function_key)
        {
            uint64_t result = static_cast<uint64_t>(component_hash);
            result ^= event_kind + MAGIC_NUMBER + (result << 6u) + (result >> 2u);
            result ^= function_key + MAGIC_NUMBER + (result << 6u) + (result >> 2u);
            return result;
        }
    }


	DeferredOps::~DeferredOps()
	{
        Clear();
        m_HookPool.Clear();
	}

	DeferredEntityHandle DeferredOps::CreateEntity(const std::string& name)
    {
        CreateEntityCommand command{};
        command.DeferredEntity = m_NextDeferredEntity++;
        command.Name = name;
        m_CreateCommands.emplace_back(std::move(command));
        return command.DeferredEntity;
    }

    DeferredEntityHandle DeferredOps::CreateEntityWithUUID(const std::string& name, const UUID& uuid)
    {
        CreateEntityCommand command{};
        command.DeferredEntity = m_NextDeferredEntity++;
        command.Name = name;
        command.ExplicitUUID = uuid;
        m_CreateCommands.emplace_back(std::move(command));
        return command.DeferredEntity;
    }

    void DeferredOps::DestroyEntity(EntityHandle entity_handle)
    {
        DestroyEntityCommand command{};
        command.Target = entity_handle;
        m_DestroyCommands.emplace_back(std::move(command));
    }

    uint64_t DeferredOps::HandleToKey(EntityHandle entity_handle)
    {
        return (static_cast<uint64_t>(entity_handle.generation) << 32u) | static_cast<uint64_t>(entity_handle.index);
    }

    DeferredOps::HookHandle DeferredOps::SubscribeOnComponentAdded(std::type_index component_type, ComponentAddedFn fn)
    {
        if (!fn)
            return HookHandle::Invalid();

        const uint64_t function_key = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(fn));
        const uint64_t dedup_key = MixHookKey(component_type.hash_code(), kHookEventAdded, function_key);
        const auto existing_it = m_HookHandleByKey.find(dedup_key);
        if (existing_it != m_HookHandleByKey.end())
            return existing_it->second;


		auto* binding = new ComponentHookBinding{};
		auto handle = m_HookPool.Add(binding);

		binding->ComponentType = component_type;
		binding->DedupKey = dedup_key;
		binding->AddedFn = fn;

		m_HookHandleByKey.emplace(dedup_key, handle);

		return handle;
    }

    DeferredOps::HookHandle DeferredOps::SubscribeOnComponentRemoved(std::type_index component_type, ComponentRemovedFn fn)
    {
        if (!fn)
            return HookHandle::Invalid();

        const uint64_t function_key = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(fn));
        const uint64_t dedup_key = MixHookKey(component_type.hash_code(), kHookEventRemoved, function_key);
        const auto existing_it = m_HookHandleByKey.find(dedup_key);
        if (existing_it != m_HookHandleByKey.end())
            return existing_it->second;

		auto* binding = new ComponentHookBinding{};
		auto handle = m_HookPool.Add(binding);

		binding->ComponentType = component_type;
		binding->DedupKey = dedup_key;
		binding->RemovedFn = fn;

		m_HookHandleByKey.emplace(dedup_key, handle);

		return handle;
    }

	bool DeferredOps::Unsubscribe(HookHandle handle)
    {
        const auto* binding = m_HookPool.Get(handle);
        if (!binding)
            return false;

        m_HookHandleByKey.erase(binding->DedupKey);
        m_HookPool.Remove(handle);
        return true;
    }

    void DeferredOps::DispatchOnComponentAdded(std::type_index component_type, World& world, EntityHandle entity_handle)
    {
        //std::vector<ComponentAddedFn> callbacks;

        m_HookPool.ForEachOccupied([&component_type, &world, &entity_handle](ComponentHookBinding* ptr) {
            if (ptr->ComponentType == component_type && ptr->AddedFn) ptr->AddedFn(world, entity_handle); });

        //for (ComponentAddedFn callback : callbacks)
        //    callback(world, entity_handle);
    }

    void DeferredOps::DispatchOnComponentRemoved(std::type_index component_type, World& world, EntityHandle entity_handle)
    {
        //std::vector<ComponentRemovedFn> callbacks;

		m_HookPool.ForEachOccupied([&component_type, &world, &entity_handle](ComponentHookBinding* ptr) {
			if (ptr->ComponentType == component_type && ptr->RemovedFn) ptr->RemovedFn(world, entity_handle); });

        //for (ComponentRemovedFn callback : callbacks)
        //    callback(world, entity_handle);
    }

	void DeferredOps::Flush(World& world, Ref<Scene> scene)
	{
		if (!scene)
		{
			Clear();
			return;
		}

		// 1: Create
		std::unordered_map<DeferredEntityHandle, EntityHandle> created_handles;
		created_handles.reserve(m_CreateCommands.size());

		for (const CreateEntityCommand& create_command : m_CreateCommands)
		{
			EntityHandle created_entity;
			if (create_command.ExplicitUUID.has_value())
				created_entity = scene->CreateEntity(create_command.Name, create_command.ExplicitUUID.value());
			else
				created_entity = scene->CreateEntity(create_command.Name);

			created_handles.emplace(create_command.DeferredEntity, created_entity);
		}

		// 2: Delete
		for (const DestroyEntityCommand& destroy_command : m_DestroyCommands)
		{
			EntityHandle entity_handle = destroy_command.Target;
			if (scene->IsEntityHandleValid(entity_handle))
				scene->DestroyEntity(entity_handle);
		}

		// 3: Add
		for (const AddComponentCommand& add_command : m_AddComponentCommands)
		{
			const DeferredOpTarget& target = add_command.Target;
			EntityHandle entity_handle;
			if (auto* handle_ptr = std::get_if<DeferredEntityHandle>(&target))
			{
				entity_handle = created_handles[*handle_ptr];
			}
			else { entity_handle = std::get<EntityHandle>(target); }

			add_command.Apply(world, scene, entity_handle);
		}

		// 4: Remove
		for (const RemoveComponentCommand& remove_command : m_RemoveComponentCommands)
		{
			const EntityHandle entity_handle = remove_command.Target;
			remove_command.Apply(world, scene, entity_handle);
		}

		// 5: CustomCommands
		for (const CustomCommand& custom_command : m_CustomCommands)
		{
			if (custom_command.Callback)
				custom_command.Callback(scene);
		}

		Clear();
	}

	void DeferredOps::Clear()
	{
		m_CreateCommands.clear();
		m_DestroyCommands.clear();
		m_AddComponentCommands.clear();
		m_RemoveComponentCommands.clear();
		m_CustomCommands.clear();
		m_NextDeferredEntity = 1;
	}
}