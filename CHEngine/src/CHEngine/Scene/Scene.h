#pragma once

#include <Core.h>
#include <CheStl/MemoryTypes.h>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <utility>
#include <tuple>
#include <boost/container_hash/hash.hpp>
#include "Components.h"
#include "Memory/HandlePool.h"


#include <entt/entt.hpp>

// Scene is a container of entities
// You must not add here another functionality except entities stuff 

namespace CHEngine {

	class Entity;
	struct EntityTag {};
	using EntityHandle = Handle<EntityTag>;

class CHENGINE_API Scene
{
public:
	Scene();
	~Scene();  // Must be declared (not defaulted here) for Scope<incomplete type>

	Scene(const Scene& other);
	Scene& operator=(const Scene& other);
	Scene(Scene&&);
	Scene& operator=(Scene&&);

	EntityHandle CreateEntity(const std::string& name = "Object");
	EntityHandle CreateEntity(const std::string& name, const UUID& uuid);

	void DestroyEntity(EntityHandle entityHandle);
	void DestroyEntity(const UUID& uuid);

	/// Iterates entities that have every type in \p Components (AND). Invokes \p fn(handle, id, comps...).
	/// Stable \p id / \p handle come from IdComponent and EntityHandlersStore (skipped if inconsistent).
	template<typename... Components, typename Fn>
	void ForEach(Fn&& fn);
	
	void Clear();

	// Entity getters
	EntityHandle TryGetEntityHandleByUUID(const UUID& uuid) const;
	const UUID GetUUID(const EntityHandle entityHandle) const;
	Entity* TryGetEntity(const EntityHandle entityHandle);
	const Entity* TryGetEntity(const EntityHandle entityHandle) const;
	bool IsEntityHandleValid(const EntityHandle entityHandle) const;

private:
	friend Entity;
	void InitializeRegistry();
	void InitSignals();
	void CloneFrom(const Scene& source);

	// On signals
	void OnTransformUpdate(entt::registry& reg, entt::entity e);

	struct SceneRegistry {
		entt::registry Registry;
		std::unordered_map<UUID, EntityHandle, boost::hash<UUID>> EntityHandlersStore;
		std::unordered_map<entt::entity, EntityHandle> EnttToHandleStore;
		HandlePool<Entity, EntityTag> EntityPool;
	};

	Scope<SceneRegistry> m_SceneRegistry;
	entt::entity TryGetEnttEntity(const EntityHandle entityHandle) const;
	EntityHandle TryGetEntityHandleByEntt(entt::entity e) const;
};

template<typename... Components, typename Fn>
void Scene::ForEach(Fn&& fn)
{
	static_assert(sizeof...(Components) >= 1u, "ForEach requires at least one component type.");

	auto& reg = m_SceneRegistry->Registry;
	for (const auto entity : reg.template view<Components...>()) {
		if (!reg.all_of<IDComponent>(entity))
			continue;

		const UUID& uuid = reg.template get<IDComponent>(entity).Value;
		const auto it = m_SceneRegistry->EntityHandlersStore.find(uuid);
		if (it == m_SceneRegistry->EntityHandlersStore.end())
			continue;

		const EntityHandle handle = it->second;

		if constexpr (sizeof...(Components) == 1u) {
			fn(handle, uuid, reg.template get<Components...>(entity));
		} else {
			std::apply([&](auto&&... comps) { fn(handle, uuid, std::forward<decltype(comps)>(comps)...); },
				reg.template get<Components...>(entity));
		}
	}
}

} // namespace CHEngine
