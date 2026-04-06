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
#include "Physics/IPhysicsWorld.h"


#include <entt/entt.hpp>

namespace CHEngine {

	class Entity;
	class PhysicsFacade;

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
	EntityHandle CreateModelEntity(const std::string& name, std::vector<Mesh>&& meshes,
	                               const std::string& sourcePath = "");
	EntityHandle CreateLightEntity(const std::string& name, LightType type);
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

	void OnUpdateRuntime(Timestep ts);
	//void OnUpdateSimulation(Timestep ts, EditorCamera& camera);
	//void OnUpdateEditor(Timestep ts, EditorCamera& camera);

	void SetPhysicsWorldDesc(const PhysicsWorldDesc& worldDesc);
	const PhysicsWorldDesc& GetPhysicsWorldDesc() const { return m_PhysicsWorldDesc; }
	bool InitPhysicsWorld();
	bool HasPhysicsWorld() const { return m_PhysicsWorld != nullptr; }

	EntityHandle CreatePhysicsEntity(const std::string& name,
	                                 const PhysicsRigidBodyDesc& bodyDesc,
	                                 const IPhysicsShape* shape,
	                                 RigidBodySyncMode syncMode = RigidBodySyncMode::Auto);
	IPhysicsBody* AttachRigidBody(EntityHandle entityHandle,
	                              const PhysicsRigidBodyDesc& bodyDesc,
	                              const IPhysicsShape* shape,
	                              RigidBodySyncMode syncMode = RigidBodySyncMode::Auto);
	IPhysicsBody* AttachRigidBody(const UUID& uuid,
	                              const PhysicsRigidBodyDesc& bodyDesc,
	                              const IPhysicsShape* shape,
	                              RigidBodySyncMode syncMode = RigidBodySyncMode::Auto);
	void DetachRigidBody(EntityHandle entityHandle);
	void DetachRigidBody(const UUID& uuid);

	IPhysicsBody* CreateRigidBody(const PhysicsRigidBodyDesc& bodyDesc, const IPhysicsShape* shape);
	void DestroyRigidBody(IPhysicsBody* body);
	bool Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, PhysicsRaycastHit& outHit);
	bool OverlapSphere(const glm::vec3& center, float radius, PhysicsOverlapResult& outResult);
	bool OverlapBox(const glm::vec3& center, const glm::vec3& halfExtents,
	                const glm::quat& rotation, PhysicsOverlapResult& outResult);
	bool OverlapCapsule(const glm::vec3& center, float radius, float halfHeight,
	                    const glm::quat& rotation, PhysicsOverlapResult& outResult);
	void SetGravity(const glm::vec3& gravity);
	void SetContactListener(IPhysicsContactListener* listener);

	// Returns the source file path (mesh import path) stored in ECS for the given object ID.
	// Returns empty string if not found. Avoids exposing entt registry publicly.
	std::string GetMeshSourcePath(const UUID& objectUUID) const;

	bool IsRunning() const { return m_IsRunning; }
	bool IsPaused() const { return m_IsPaused; }

private:
	friend Entity;
	friend PhysicsFacade;

	struct SceneRegistry {
		entt::registry Registry;
		std::unordered_map<UUID, EntityHandle, boost::hash<UUID>> EntityHandlersStore;
		HandlePool<Entity, EntityTag> EntityPool;
	};

	void OnEntityDestroyed(EntityHandle entityHandle);
	bool IsEnttEntityValid(entt::entity entity) const;
	bool IsEntityBoundToHandle(entt::entity entity) const;
	void DestroyAttachedRigidBody(entt::entity entity);

	void SyncTransformsToPhysics();
	void SyncTransformsFromPhysics();

	bool m_IsRunning = false;
	bool m_IsPaused = false;

	Scope<SceneRegistry> m_SceneRegistry;
	PhysicsWorldDesc m_PhysicsWorldDesc{};
	Scope<IPhysicsWorld> m_PhysicsWorld;

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
