#pragma once

#include <Core.h>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <utility>
#include "SceneObject.h"
#include "Entity.h"

namespace CHEngine {

class CHENGINE_API Scene
{
public:
	Scene();
	~Scene();  // Must be declared (not defaulted here) for unique_ptr<incomplete type>

	Scene(const Scene&) = delete;
	Scene& operator=(const Scene&) = delete;
	Scene(Scene&&);
	Scene& operator=(Scene&&);

	Entity CreateEntity(const std::string& name = "Object");
	void DestroyEntity(Entity entity);
	Entity GetEntityByObjectID(uint32_t id);
	Entity GetEntityByObjectID(uint32_t id) const;

	template<typename T, typename... Args>
	T& AddComponent(Entity entity, Args&&... args);

	template<typename T>
	T& GetComponent(Entity entity);

	template<typename T>
	const T& GetComponent(Entity entity) const;

	template<typename T>
	bool HasComponent(Entity entity) const;

	template<typename T>
	void RemoveComponent(Entity entity);

	SceneObject* AddObject(const std::string& name = "Object");
	SceneObject* AddModel(const std::string& name, std::vector<Mesh>&& meshes,
	                      const std::string& sourcePath = "");
	SceneObject* AddLight(const std::string& name, LightType type);
	void RemoveObject(uint32_t id);
	SceneObject* FindByID(uint32_t id);
	void Clear();

	std::vector<std::unique_ptr<SceneObject>>& GetObjects() { return m_Objects; }
	const std::vector<std::unique_ptr<SceneObject>>& GetObjects() const { return m_Objects; }

	// Returns the source file path (mesh import path) stored in ECS for the given object ID.
	// Returns empty string if not found. Avoids exposing entt registry publicly.
	std::string GetMeshSourcePath(uint32_t objectID) const;

private:
	struct SceneRegistry;

	Entity CreateEntityWithID(const std::string& name, uint32_t id);
	void OnEntityDestroyed(Entity entity);

	std::vector<std::unique_ptr<SceneObject>> m_Objects;
	std::unordered_map<uint32_t, SceneObject*> m_IDIndex; // O(1) lookup by ID
	std::unique_ptr<SceneRegistry> m_SceneRegistry;
};

template<typename T, typename... Args>
T& Entity::AddComponent(Args&&... args)
{
	CHE_CORE_ASSERT(m_Scene, "Entity has no owning scene");
	return m_Scene->AddComponent<T>(*this, std::forward<Args>(args)...);
}

template<typename T>
T& Entity::GetComponent()
{
	CHE_CORE_ASSERT(m_Scene, "Entity has no owning scene");
	return m_Scene->GetComponent<T>(*this);
}

template<typename T>
bool Entity::HasComponent() const
{
	CHE_CORE_ASSERT(m_Scene, "Entity has no owning scene");
	return m_Scene->HasComponent<T>(*this);
}

template<typename T>
void Entity::RemoveComponent()
{
	CHE_CORE_ASSERT(m_Scene, "Entity has no owning scene");
	m_Scene->RemoveComponent<T>(*this);
}

} // namespace CHEngine
