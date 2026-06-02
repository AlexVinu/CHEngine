#pragma once

#include <Core.h>
#include <CheStl/MemoryTypes.h>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <unordered_map>
#include <utility>
#include <tuple>
#include "Components.h"
#include "Memory/HandlePool.h"


#include <entt/entt.hpp>

// Scene is a container of entities
// You must not add here another functionality except entities stuff 

namespace CHEngine {

	class Entity; class SceneSerializer;
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

	// World-level scripts attached to the scene (not bound to any entity).
	// Loaded by LuaScriptSystem alongside per-entity ScriptComponent scripts.
	std::vector<ScriptEntry> WorldScripts;

	// ── String pool ──────────────────────────────────────────────────────────
	// StringID 0 = empty string.
	StringID           InternString(const std::string& str);
	const std::string& GetString(StringID id) const;

	// ── Script pool ──────────────────────────────────────────────────────────
	// ScriptsID 0 = no scripts.  AllocateScripts() creates an empty pool entry.
	ScriptsID                      AllocateScripts();
	const std::vector<ScriptEntry>* GetScripts(ScriptsID id) const;
	std::vector<ScriptEntry>*       GetScripts(ScriptsID id);

	// Read-only pool iterators (used by SceneSerializer).
	const std::unordered_map<StringID, std::string>&               GetStringPool()  const { return StringPool; }
	const std::unordered_map<ScriptsID, std::vector<ScriptEntry>>& GetScriptPool()  const { return ScriptPool; }

	// Direct pool restoration (used by SceneSerializer when loading v4 data).
	void LoadStringPoolEntry(StringID id, const std::string& str);
	void LoadScriptPoolEntry(ScriptsID id, std::vector<ScriptEntry> scripts);

	// Mark-and-sweep cleanup of the string/script pools: walks every component
	void CollectGarbage();

	EntityHandle CreateEntity(const std::string& name = "Object");
	EntityHandle CreateEntity(const std::string& name, const UUID& uuid);

	void DestroyEntity(EntityHandle entityHandle);
	void DestroyEntity(const UUID& uuid);

	/// Iterates entities that have every type in \p Components (AND). Invokes \p fn(handle, id, comps...).
	/// Stable \p id / \p handle come from IdComponent and EntityHandlersStore (skipped if inconsistent).
	template<typename... Components, typename Fn>
	void ForEach(Fn&& fn);

	template<typename Fn>
	void ForEach(Fn&& fn);
	
	void Clear();

	// Entity getters
	EntityHandle TryGetEntityHandleByUUID(const UUID& uuid) const;
	const UUID GetUUID(const EntityHandle entityHandle) const;
	const UUID GetUUIDByString(const std::string_view name) const;
	Entity* TryGetEntity(const EntityHandle entityHandle);
	const Entity* TryGetEntity(const EntityHandle entityHandle) const;
	bool IsEntityHandleValid(const EntityHandle entityHandle) const;

    // Internal access for cross-world entity transfer (DeferredOps).
    entt::registry& GetRegistryRef() { return m_SceneRegistry->Registry; }
    const entt::registry& GetRegistryRef() const { return m_SceneRegistry->Registry; }

private:

	std::unordered_map<StringID,  std::string>             StringPool;
	std::unordered_map<ScriptsID, std::vector<ScriptEntry>> ScriptPool;

	struct string_hash {
		using is_transparent = void; 

		size_t operator()(std::string_view sv) const {
			return std::hash<std::string_view>{}(sv);
		}
		size_t operator()(const std::string& s) const {
			return std::hash<std::string>{}(s);
		}
		size_t operator()(const char* s) const {
			return std::hash<std::string_view>{}(s);
		}
	};
	std::unordered_map<std::string, StringID, string_hash, std::equal_to<>> m_StringLookup;
	StringID  m_NextStringID  = 0;
	ScriptsID m_NextScriptsID = 0;

	friend Entity; friend SceneSerializer;
	void InitializeRegistry();
	void InitSignals();
	void CloneFrom(const Scene& source);

	// On signals
	void OnTransformUpdate(entt::registry& reg, entt::entity e);

	struct SceneRegistry {
		entt::registry Registry;
		std::unordered_map<UUID, EntityHandle> EntityHandlersStore;
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
template<typename Fn>
void Scene::ForEach(Fn&& fn)
{
	auto& reg = m_SceneRegistry->Registry;
	for (auto entity : reg.template storage<entt::entity>())
		fn(entity);
}

} // namespace CHEngine
