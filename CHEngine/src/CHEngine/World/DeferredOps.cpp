#include "chepch.h"
#include "DeferredOps.h"
#include "World.h"
#include "WorldsList.h"

#include <entt/entt.hpp>

#include <cstdint>
#include <unordered_set>
#include <vector>

namespace CHEngine
{
    namespace
    {
        constexpr uint64_t kHookEventAdded       = 0xA11Du;
        constexpr uint64_t kHookEventRemoved      = 0xB22Eu;
        constexpr uint64_t kHookEventTransferOut  = 0xC33Fu;
        constexpr uint64_t kHookEventTransferIn   = 0xD440u;

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

    DeferredOps::HookHandle DeferredOps::SubscribeOnComponentTransferOut(std::type_index component_type, uint32_t entt_type_id, ComponentTransferOutFn fn)
    {
        if (!fn)
            return HookHandle::Invalid();

        const uint64_t function_key = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(fn));
        const uint64_t dedup_key = MixHookKey(component_type.hash_code(), kHookEventTransferOut, function_key);
        const auto existing_it = m_HookHandleByKey.find(dedup_key);
        if (existing_it != m_HookHandleByKey.end())
            return existing_it->second;

        auto* binding = new ComponentHookBinding{};
        auto handle = m_HookPool.Add(binding);
        binding->ComponentType = component_type;
        binding->EnttTypeId    = entt_type_id;
        binding->DedupKey      = dedup_key;
        binding->TransferOutFn = fn;
        m_HookHandleByKey.emplace(dedup_key, handle);
        return handle;
    }

    DeferredOps::HookHandle DeferredOps::SubscribeOnComponentTransferIn(std::type_index component_type, uint32_t entt_type_id, ComponentTransferInFn fn)
    {
        if (!fn)
            return HookHandle::Invalid();

        const uint64_t function_key = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(fn));
        const uint64_t dedup_key = MixHookKey(component_type.hash_code(), kHookEventTransferIn, function_key);
        const auto existing_it = m_HookHandleByKey.find(dedup_key);
        if (existing_it != m_HookHandleByKey.end())
            return existing_it->second;

        auto* binding = new ComponentHookBinding{};
        auto handle = m_HookPool.Add(binding);
        binding->ComponentType = component_type;
        binding->EnttTypeId    = entt_type_id;
        binding->DedupKey      = dedup_key;
        binding->TransferInFn  = fn;
        m_HookHandleByKey.emplace(dedup_key, handle);
        return handle;
    }

    void DeferredOps::TransferEntity(EntityHandle entity, std::string_view dstWorldName)
    {
        TransferEntityCommand cmd{};
        cmd.Source      = entity;
        cmd.DstWorldName = std::string(dstWorldName);
        m_TransferCommands.emplace_back(std::move(cmd));
    }

    void DeferredOps::FireTransferOutHooks(uint32_t entt_type_id, World& src, EntityHandle handle, TransferContext& ctx)
    {
        m_HookPool.ForEachOccupied([&](ComponentHookBinding* ptr) {
            if (ptr->EnttTypeId == entt_type_id && ptr->TransferOutFn)
                ptr->TransferOutFn(src, handle, ctx);
        });
    }

    void DeferredOps::FireTransferInHooks(uint32_t entt_type_id, World& dst, EntityHandle handle, const TransferContext& ctx)
    {
        m_HookPool.ForEachOccupied([&](ComponentHookBinding* ptr) {
            if (ptr->EnttTypeId == entt_type_id && ptr->TransferInFn)
                ptr->TransferInFn(dst, handle, ctx);
        });
    }

    void DeferredOps::TransferSubtree(World& srcWorld, Ref<Scene> srcScene,
                                       World& dstWorld, Ref<Scene> dstScene,
                                       EntityHandle rootHandle)
    {
        using namespace entt::literals;

        bool transferredActiveCamera = false;

        // BFS: collect subtree (root first, then children).
        std::vector<EntityHandle> subtree;
        std::vector<EntityHandle> frontier = { rootHandle };

        while (!frontier.empty())
        {
            std::vector<EntityHandle> next;
            for (EntityHandle h : frontier)
            {
                subtree.push_back(h);
                const UUID parentUUID = srcScene->GetUUID(h);
                // Find all entities whose ParentNodeComponent.Value == parentUUID.
                srcScene->ForEach<ParentNodeComponent>([&](EntityHandle ch, UUID, ParentNodeComponent& pnc) {
                    if (pnc.Value == parentUUID)
                        next.push_back(ch);
                });
            }
            frontier = std::move(next);
        }

        // Transfer each entity: parents first (subtree ordering is already correct).
        for (EntityHandle srcHandle : subtree)
        {
            const UUID uuid     = srcScene->GetUUID(srcHandle);
            const Entity* srcEnt = srcScene->TryGetEntity(srcHandle);
            if (!srcEnt) continue;

            const std::string name = srcEnt->HasComponent<TagComponent>()
                ? srcScene->GetString(srcEnt->GetComponent<TagComponent>().Name)
                : "Object";

            entt::registry& srcReg = srcScene->GetRegistryRef();
            entt::entity srcE = static_cast<entt::entity>(srcHandle.index); // handled via entt mapping below

            // Resolve entt entity via Scene internals (use UUID lookup on dst side).
            // For src: we need the raw entt entity. Use registry.storage iteration from handle.
            // Approach: get entt entity by looking up via Scene's handle pool.
            // We don't have direct entt::entity from EntityHandle, so iterate through
            // IDComponent to find the matching entity.
            entt::entity srcEntt = entt::null;
            {
                auto view = srcReg.view<IDComponent>();
                for (auto e : view)
                    if (view.get<IDComponent>(e).Value == uuid)
                    { srcEntt = e; break; }
            }
            if (srcEntt == entt::null) continue;

            // Create the entity in dst with the same UUID.
            EntityHandle dstHandle = dstScene->CreateEntity(name, uuid);
            entt::registry& dstReg = dstScene->GetRegistryRef();

            entt::entity dstEntt = entt::null;
            {
                auto view = dstReg.view<IDComponent>();
                for (auto e : view)
                    if (view.get<IDComponent>(e).Value == uuid)
                    { dstEntt = e; break; }
            }
            if (dstEntt == entt::null) continue;

            TransferContext ctx;

            // Collect which entt type ids were actually transferred.
            std::vector<uint32_t> transferredTypeIds;

            // Pass 1: FireTransferOut + Clone all components into dst.
            // TransferIn is deferred until all components are present in dst.
            for (auto&& [typeId, storage] : srcReg.storage())
            {
                if (!storage.contains(srcEntt)) continue;

                // Skip IDComponent and TagComponent — CreateEntity already set these.
                const auto& typeInfo = storage.type();
                if (typeInfo == entt::type_id<IDComponent>() ||
                    typeInfo == entt::type_id<TagComponent>())
                    continue;

                // Track whether an active camera is leaving the src world.
                if (typeInfo == entt::type_id<CameraComponent>())
                {
                    const auto& cam = srcReg.get<CameraComponent>(srcEntt);
                    if (cam.IsActive)
                        transferredActiveCamera = true;
                }

                const uint32_t enttTypeId = static_cast<uint32_t>(typeInfo.hash());

                // 1. Fire TransferOut hooks on src.
                FireTransferOutHooks(enttTypeId, srcWorld, srcHandle, ctx);

                // 2. Clone component into dst via entt::meta "clone_to" function.
                auto metaType = entt::resolve(typeInfo);
                if (metaType)
                {
                    auto cloneFn = metaType.func("clone_to"_hs);
                    if (cloneFn)
                        cloneFn.invoke({},
                            entt::forward_as_meta(dstReg),
                            entt::forward_as_meta(dstEntt),
                            entt::forward_as_meta(srcReg),
                            entt::forward_as_meta(srcEntt));
                }

                transferredTypeIds.push_back(enttTypeId);
            }

            // Pass 2: Fire TransferIn hooks — all components are now present in dst.
            for (uint32_t enttTypeId : transferredTypeIds)
                dstWorld.GetDeferredOps().FireTransferInHooks(enttTypeId, dstWorld, dstHandle, ctx);

            // Destroy the entity in src immediately (we're outside any ForEach here).
            srcScene->DestroyEntity(srcHandle);
        }

        // If the subtree carried the active camera, adjust world states.
        // SetState() only sets m_PendingState — applied safely at next World::Simulate().
        if (transferredActiveCamera)
        {
            srcWorld.SetState(WorldState::SimulatingWithoutPresenting);
            if (dstWorld.GetState() == WorldState::SimulatingWithoutPresenting)
                dstWorld.SetState(WorldState::Simulating);
        }
    }

    void DeferredOps::FlushTransfers(World& world, Ref<Scene> scene)
    {
        if (m_TransferCommands.empty()) return;

        WorldsList* worlds = world.GetWorlds();
        if (!worlds)
        {
            CHE_CORE_WARN("DeferredOps::FlushTransfers: World '{}' is not registered in a Worlds index — transfers ignored.",
                          world.GetWorldName());
            m_TransferCommands.clear();
            return;
        }

        const WorldState srcState = world.GetState();
        const bool srcOk = srcState == WorldState::Simulating || srcState == WorldState::SimulatingWithoutPresenting;

        for (const TransferEntityCommand& cmd : m_TransferCommands)
        {
            if (!srcOk)
            {
                CHE_CORE_ERROR("DeferredOps::FlushTransfers: src World '{}' is not Simulating — transfer dropped.",
                               world.GetWorldName());
                continue;
            }

            World* dst = worlds->TryGet(cmd.DstWorldName);
            if (!dst)
            {
                CHE_CORE_ERROR("DeferredOps::FlushTransfers: dst World '{}' not found — transfer dropped.",
                               cmd.DstWorldName);
                continue;
            }

            if (dst == &world)
            {
                CHE_CORE_WARN("DeferredOps::FlushTransfers: src and dst are the same World '{}' — skipped.",
                              world.GetWorldName());
                continue;
            }

            const WorldState dstState = dst->GetState();
            if (dstState != WorldState::Simulating && dstState != WorldState::SimulatingWithoutPresenting)
            {
                CHE_CORE_ERROR("DeferredOps::FlushTransfers: dst World '{}' is not Simulating — transfer dropped.",
                               cmd.DstWorldName);
                continue;
            }

            Ref<Scene> dstScene = dst->GetSceneRef();
            if (!dstScene)
            {
                CHE_CORE_ERROR("DeferredOps::FlushTransfers: dst World '{}' has no scene — transfer dropped.",
                               cmd.DstWorldName);
                continue;
            }

            if (!scene->IsEntityHandleValid(cmd.Source))
            {
                CHE_CORE_ERROR("DeferredOps::FlushTransfers: entity handle is invalid — transfer dropped.");
                continue;
            }

            TransferSubtree(world, scene, *dst, dstScene, cmd.Source);
        }

        m_TransferCommands.clear();
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

		// 6: Cross-world entity transfers (happens last so all scene state is stable).
		FlushTransfers(world, scene);

		Clear();
	}
    // Calls every frame
	void DeferredOps::Clear()
	{
		m_CreateCommands.clear();
		m_DestroyCommands.clear();
		m_AddComponentCommands.clear();
		m_RemoveComponentCommands.clear();
		m_CustomCommands.clear();
		m_TransferCommands.clear();
		m_NextDeferredEntity = 1;
	}
}
