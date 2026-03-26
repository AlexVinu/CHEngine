#include "chepch.h"
#include "Scene.h"
#include "Components.h"
#include <algorithm>
#include <entt/entt.hpp>

namespace CHEngine {

struct Scene::SceneRegistry
{
	entt::registry Registry;
	std::unordered_map<uint32_t, entt::entity> ObjectToEntity;
};

Scene::Scene()
	: m_SceneRegistry(std::make_unique<SceneRegistry>())
{
}

Scene::~Scene() = default;

Scene::Scene(Scene&&) = default;
Scene& Scene::operator=(Scene&&) = default;

Entity Scene::CreateEntityWithID(const std::string& name, uint32_t id)
{
	Entity entity{m_SceneRegistry->Registry.create(), this};
	entity.AddComponent<TagComponent>(TagComponent{name, id});
	entity.AddComponent<TransformComponent>();
	entity.AddComponent<MeshComponent>();
	entity.AddComponent<ColorComponent>();
	entity.AddComponent<VisibilityComponent>();
	m_SceneRegistry->ObjectToEntity[id] = entity.GetHandle();
	return entity;
}

Entity Scene::CreateEntity(const std::string& name)
{
	SceneObject* object = AddObject(name);
	return GetEntityByObjectID(object->ID);
}

void Scene::OnEntityDestroyed(Entity entity)
{
	if (!entity || !m_SceneRegistry->Registry.valid(entity.GetHandle()))
		return;

	if (entity.HasComponent<TagComponent>()) {
		const uint32_t objectID = entity.GetComponent<TagComponent>().ID;
		m_SceneRegistry->ObjectToEntity.erase(objectID);
		m_IDIndex.erase(objectID);
		m_Objects.erase(
			std::remove_if(m_Objects.begin(), m_Objects.end(),
				[objectID](const std::unique_ptr<SceneObject>& obj) { return obj->ID == objectID; }),
			m_Objects.end()
		);
	}

	m_SceneRegistry->Registry.destroy(entity.GetHandle());
}

void Scene::DestroyEntity(Entity entity)
{
	OnEntityDestroyed(entity);
}

Entity Scene::GetEntityByObjectID(uint32_t id)
{
	auto it = m_SceneRegistry->ObjectToEntity.find(id);
	if (it != m_SceneRegistry->ObjectToEntity.end() && m_SceneRegistry->Registry.valid(it->second))
		return Entity{it->second, this};

	auto view = m_SceneRegistry->Registry.view<TagComponent>();
	for (auto handle : view) {
		if (view.get<TagComponent>(handle).ID == id) {
			m_SceneRegistry->ObjectToEntity[id] = handle;
			return Entity{handle, this};
		}
	}

	return {};
}

Entity Scene::GetEntityByObjectID(uint32_t id) const
{
	return const_cast<Scene*>(this)->GetEntityByObjectID(id);
}

SceneObject* Scene::AddObject(const std::string& name)
{
	auto obj = std::make_unique<SceneObject>(name);
	SceneObject* ptr = obj.get();
	m_IDIndex[ptr->ID] = ptr;
	m_Objects.push_back(std::move(obj));

	Entity entity = CreateEntityWithID(ptr->Name, ptr->ID);
	auto& transform = entity.GetComponent<TransformComponent>().ObjectTransform;
	transform = ptr->ObjectTransform;
	entity.GetComponent<ColorComponent>().Color = ptr->Color;
	entity.GetComponent<VisibilityComponent>().Visible = ptr->Visible;

	return ptr;
}

SceneObject* Scene::AddLight(const std::string& name, LightType type)
{
	auto obj = std::make_unique<SceneObject>(name);
	obj->LightData.Type = type;
	SceneObject* ptr = obj.get();
	m_IDIndex[ptr->ID] = ptr;
	m_Objects.push_back(std::move(obj));

	Entity entity = CreateEntityWithID(ptr->Name, ptr->ID);
	auto& transform = entity.GetComponent<TransformComponent>().ObjectTransform;
	transform = ptr->ObjectTransform;
	entity.GetComponent<ColorComponent>().Color = ptr->Color;
	entity.GetComponent<VisibilityComponent>().Visible = ptr->Visible;
	entity.AddComponent<LightComponent>(LightComponent{ptr->LightData});

	return ptr;
}

SceneObject* Scene::AddModel(const std::string& name, std::vector<Mesh>&& meshes,
                             const std::string& sourcePath)
{
	auto obj = std::make_unique<SceneObject>(name);
	obj->Meshes = std::move(meshes);
	SceneObject* ptr = obj.get();
	m_IDIndex[ptr->ID] = ptr;
	m_Objects.push_back(std::move(obj));

	Entity entity = CreateEntityWithID(ptr->Name, ptr->ID);
	auto& transform = entity.GetComponent<TransformComponent>().ObjectTransform;
	transform = ptr->ObjectTransform;
	auto& mc = entity.GetComponent<MeshComponent>();
	mc.Meshes = ptr->Meshes;
	mc.SourcePath = sourcePath;
	entity.GetComponent<ColorComponent>().Color = ptr->Color;
	entity.GetComponent<VisibilityComponent>().Visible = ptr->Visible;

	return ptr;
}

void Scene::RemoveObject(uint32_t id)
{
	DestroyEntity(GetEntityByObjectID(id));
}

SceneObject* Scene::FindByID(uint32_t id)
{
	auto it = m_IDIndex.find(id);
	return (it != m_IDIndex.end()) ? it->second : nullptr;
}

void Scene::Clear()
{
	m_Objects.clear();
	m_IDIndex.clear();
	m_SceneRegistry->ObjectToEntity.clear();
	m_SceneRegistry->Registry.clear();
}

std::string Scene::GetMeshSourcePath(uint32_t objectID) const
{
	auto view = m_SceneRegistry->Registry.view<TagComponent, MeshComponent>();
	for (auto e : view) {
		if (view.get<TagComponent>(e).ID == objectID)
			return view.get<MeshComponent>(e).SourcePath;
	}
	return {};
}

template<typename T, typename... Args>
T& Scene::AddComponent(Entity entity, Args&&... args)
{
	CHE_CORE_ASSERT(entity, "AddComponent called for invalid entity");
	CHE_CORE_ASSERT(!entity.HasComponent<T>(), "Entity already has this component");
	return m_SceneRegistry->Registry.emplace<T>(entity.GetHandle(), std::forward<Args>(args)...);
}

template<typename T>
T& Scene::GetComponent(Entity entity)
{
	CHE_CORE_ASSERT(entity, "GetComponent called for invalid entity");
	CHE_CORE_ASSERT(entity.HasComponent<T>(), "Entity does not have this component");
	return m_SceneRegistry->Registry.get<T>(entity.GetHandle());
}

template<typename T>
const T& Scene::GetComponent(Entity entity) const
{
	CHE_CORE_ASSERT(entity, "GetComponent called for invalid entity");
	CHE_CORE_ASSERT(entity.HasComponent<T>(), "Entity does not have this component");
	return m_SceneRegistry->Registry.get<T>(entity.GetHandle());
}

template<typename T>
bool Scene::HasComponent(Entity entity) const
{
	return entity && m_SceneRegistry->Registry.all_of<T>(entity.GetHandle());
}

template<typename T>
void Scene::RemoveComponent(Entity entity)
{
	CHE_CORE_ASSERT(entity, "RemoveComponent called for invalid entity");
	CHE_CORE_ASSERT(entity.HasComponent<T>(), "Entity does not have this component");
	m_SceneRegistry->Registry.remove<T>(entity.GetHandle());
}

template TagComponent& Scene::AddComponent<TagComponent>(Entity, TagComponent&&);
template TransformComponent& Scene::AddComponent<TransformComponent>(Entity);
template MeshComponent& Scene::AddComponent<MeshComponent>(Entity);
template ColorComponent& Scene::AddComponent<ColorComponent>(Entity);
template VisibilityComponent& Scene::AddComponent<VisibilityComponent>(Entity);
template LightComponent& Scene::AddComponent<LightComponent>(Entity, LightComponent&&);

template TagComponent& Scene::GetComponent<TagComponent>(Entity);
template TransformComponent& Scene::GetComponent<TransformComponent>(Entity);
template MeshComponent& Scene::GetComponent<MeshComponent>(Entity);
template ColorComponent& Scene::GetComponent<ColorComponent>(Entity);
template VisibilityComponent& Scene::GetComponent<VisibilityComponent>(Entity);
template const TagComponent& Scene::GetComponent<TagComponent>(Entity) const;
template const TransformComponent& Scene::GetComponent<TransformComponent>(Entity) const;
template const MeshComponent& Scene::GetComponent<MeshComponent>(Entity) const;
template const ColorComponent& Scene::GetComponent<ColorComponent>(Entity) const;
template const VisibilityComponent& Scene::GetComponent<VisibilityComponent>(Entity) const;

template bool Scene::HasComponent<TagComponent>(Entity) const;
template bool Scene::HasComponent<TransformComponent>(Entity) const;
template bool Scene::HasComponent<MeshComponent>(Entity) const;
template bool Scene::HasComponent<ColorComponent>(Entity) const;
template bool Scene::HasComponent<VisibilityComponent>(Entity) const;
template bool Scene::HasComponent<LightComponent>(Entity) const;

template void Scene::RemoveComponent<TagComponent>(Entity);
template void Scene::RemoveComponent<TransformComponent>(Entity);
template void Scene::RemoveComponent<MeshComponent>(Entity);
template void Scene::RemoveComponent<ColorComponent>(Entity);
template void Scene::RemoveComponent<VisibilityComponent>(Entity);
template void Scene::RemoveComponent<LightComponent>(Entity);

} // namespace CHEngine
