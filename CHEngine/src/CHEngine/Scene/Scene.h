#pragma once

#include <Core.h>
#include <CheStl/MemoryTypes.h>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <utility>
#include <boost/container_hash/hash.hpp>
#include "Components.h"
#include "Memory/HandlePool.h"


#include <entt/entt.hpp>

namespace CHEngine {

	class Entity;
	struct EntityTag {};
	using EntityHandle = Handle<EntityTag>;

class CHENGINE_API Scene
{
public:
	Scene();
	~Scene();  // Must be declared (not defaulted here) for Scope<incomplete type>

	Scene(const Scene&) = delete;
	Scene& operator=(const Scene&) = delete;
	Scene(Scene&&);
	Scene& operator=(Scene&&);

	EntityHandle CreateEntity(const std::string& name = "Object");
	EntityHandle CreateEntity(const std::string& name, const UUID& uuid);
	void DestroyEntity(EntityHandle entityHandle);
	EntityHandle TryGetEntityHandleByUUID(const UUID& uuid) const;
	UUID GetUUID(EntityHandle entityHandle) const;
	Entity* TryGetEntity(EntityHandle entityHandle);
	const Entity* TryGetEntity(EntityHandle entityHandle) const;
	bool IsEntityHandleValid(EntityHandle entityHandle) const;

	/// Iterates entities that have \p Component. Invokes \p fn(handle, id, comp).
	/// Stable \p id / \p handle come from TagComponent and EntityHandlersStore (skipped if inconsistent).
	template<typename Component, typename Fn>
	void ForEach(Fn&& fn);

	template<typename Component, typename Fn>
	void ForEach(Fn&& fn) const;

	template<typename T>
	T* TryGetComponent(EntityHandle entityHandle);

	template<typename T>
	const T* TryGetComponent(EntityHandle entityHandle) const;

	void SetLightComponent(EntityHandle entityHandle, const Light& light);
	void RemoveLightComponent(EntityHandle entityHandle);

	void RemoveObject(const UUID& uuid);
	void Clear();

	// Returns the source file path (mesh import path) stored in ECS for the given object ID.
	// Returns empty string if not found. Avoids exposing entt registry publicly.
	std::string GetMeshSourcePath(const UUID& objectUUID) const;

private:
	friend Entity;

	struct SceneRegistry {
		entt::registry Registry;
		std::unordered_map<UUID, EntityHandle, boost::hash<UUID>> EntityHandlersStore;
		HandlePool<Entity, EntityTag> EntityPool;
	};

	void OnEntityDestroyed(EntityHandle entityHandle);
	bool IsEnttEntityValid(entt::entity entity) const;
	bool IsEntityBoundToHandle(entt::entity entity) const;

	Scope<SceneRegistry> m_SceneRegistry;

	template<typename T, typename... Args>
	T& AddComponent(entt::entity entity, Args&&... args);

	template<typename T>
	T& GetComponent(entt::entity entity);

	template<typename T>
	const T& GetComponent(entt::entity entity) const;

	template<typename T>
	bool HasComponent(entt::entity entity) const;

	template<typename T>
	void RemoveComponent(entt::entity entity);
};

template<typename Component, typename Fn>
void Scene::ForEach(Fn&& fn)
{
	auto& reg = m_SceneRegistry->Registry;
	for (const auto entity : reg.view<Component>()) {
		if (!reg.all_of<IDComponent>(entity))
			continue;

		const UUID& uuid = reg.template get<IDComponent>(entity).Value;
		const auto it = m_SceneRegistry->EntityHandlersStore.find(uuid);
		if (it == m_SceneRegistry->EntityHandlersStore.end())
			continue;

		const EntityHandle handle = it->second;
		if (!TryGetEntity(handle))
			continue;

		fn(handle, uuid, reg.template get<Component>(entity));
	}
}

template<typename Component, typename Fn>
void Scene::ForEach(Fn&& fn) const
{
	const auto& reg = m_SceneRegistry->Registry;
	for (const auto entity : reg.view<Component>()) {
		if (!reg.all_of<IDComponent>(entity))
			continue;

		const UUID& uuid = reg.template get<IDComponent>(entity).Value;
		const auto it = m_SceneRegistry->EntityHandlersStore.find(uuid);
		if (it == m_SceneRegistry->EntityHandlersStore.end())
			continue;

		const EntityHandle handle = it->second;
		if (!TryGetEntity(handle))
			continue;

		fn(handle, uuid, reg.template get<Component>(entity));
	}
}

} // namespace CHEngine
