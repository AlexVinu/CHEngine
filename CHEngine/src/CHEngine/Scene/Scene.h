#pragma once

#include <Core.h>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <utility>
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
	~Scene();  // Must be declared (not defaulted here) for unique_ptr<incomplete type>

	Scene(const Scene&) = delete;
	Scene& operator=(const Scene&) = delete;
	Scene(Scene&&);
	Scene& operator=(Scene&&);

	EntityHandle CreateEntity(const std::string& name = "Object");
	EntityHandle CreateEntity(const std::string& name, TagComponentIDType id);
	EntityHandle CreateModelEntity(const std::string& name, std::vector<Mesh>&& meshes,
	                               const std::string& sourcePath = "");
	EntityHandle CreateLightEntity(const std::string& name, LightType type);
	void DestroyEntity(EntityHandle entityHandle);
	EntityHandle TryGetEntityHandleByID(TagComponentIDType id) const;
	TagComponentIDType GetID(EntityHandle entityHandle) const;
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

	void RemoveObject(TagComponentIDType id);
	void Clear();

	// Returns the source file path (mesh import path) stored in ECS for the given object ID.
	// Returns empty string if not found. Avoids exposing entt registry publicly.
	std::string GetMeshSourcePath(TagComponentIDType objectID) const;

private:
	friend Entity;

	struct SceneRegistry {
		entt::registry Registry;
		std::unordered_map<TagComponentIDType, EntityHandle> EntityHandlersStore;
		HandlePool<Entity, EntityTag> EntityPool;
		TagComponentIDType NextID = 1;
	};

	void OnEntityDestroyed(EntityHandle entityHandle);
	bool IsEnttEntityValid(entt::entity entity) const;

	std::unique_ptr<SceneRegistry> m_SceneRegistry;

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
		if (!reg.all_of<TagComponent>(entity))
			continue;

		const TagComponentIDType id = reg.template get<TagComponent>(entity).ID;
		const auto it = m_SceneRegistry->EntityHandlersStore.find(id);
		if (it == m_SceneRegistry->EntityHandlersStore.end())
			continue;

		const EntityHandle handle = it->second;
		if (!TryGetEntity(handle))
			continue;

		fn(handle, id, reg.template get<Component>(entity));
	}
}

template<typename Component, typename Fn>
void Scene::ForEach(Fn&& fn) const
{
	const auto& reg = m_SceneRegistry->Registry;
	for (const auto entity : reg.view<Component>()) {
		if (!reg.all_of<TagComponent>(entity))
			continue;

		const TagComponentIDType id = reg.template get<TagComponent>(entity).ID;
		const auto it = m_SceneRegistry->EntityHandlersStore.find(id);
		if (it == m_SceneRegistry->EntityHandlersStore.end())
			continue;

		const EntityHandle handle = it->second;
		if (!TryGetEntity(handle))
			continue;

		fn(handle, id, reg.template get<Component>(entity));
	}
}

} // namespace CHEngine
