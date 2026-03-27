#include "chepch.h"
#include "Scene.h"
#include "Entity.h"
#include "Components.h"

#include <entt/entt.hpp>

namespace CHEngine {

	Scene::Scene()
		: m_SceneRegistry(std::make_unique<SceneRegistry>())
	{
		m_SceneRegistry->EntityPool = HandlePool<Entity, EntityTag>([](Entity* ptr) { delete ptr; });
	}

	Scene::~Scene() = default;

	Scene::Scene(Scene&&) = default;
	Scene& Scene::operator=(Scene&&) = default;

	EntityHandle Scene::CreateEntity(const std::string& name, TagComponentIDType id)
	{
		CHE_CORE_ASSERT(m_SceneRegistry->EntityHandlersStore.find(id) == m_SceneRegistry->EntityHandlersStore.end(),
		                "CreateEntity: duplicate TagComponent ID");
		auto* entity = new Entity{ m_SceneRegistry->Registry.create(), this };
		const EntityHandle handle = m_SceneRegistry->EntityPool.Add(entity);
		entity->AddComponent<TagComponent>(TagComponent{ name, id });
		entity->AddComponent<TransformComponent>();
		entity->AddComponent<MeshComponent>();
		entity->AddComponent<ColorComponent>();
		entity->AddComponent<VisibilityComponent>();
		m_SceneRegistry->EntityHandlersStore[id] = handle;
		m_SceneRegistry->NextID = std::max(m_SceneRegistry->NextID, static_cast<TagComponentIDType>(id + 1));

		return handle;
	}

	EntityHandle Scene::CreateEntity(const std::string& name)
	{
		return CreateEntity(name, m_SceneRegistry->NextID++);
	}

	EntityHandle Scene::CreateModelEntity(const std::string& name, std::vector<Mesh>&& meshes,
	                                      const std::string& sourcePath)
	{
		const EntityHandle handle = CreateEntity(name);
		if (Entity* entity = TryGetEntity(handle)) {
			auto& mesh = entity->GetComponent<MeshComponent>();
			mesh.Meshes = std::move(meshes);
			mesh.SourcePath = sourcePath;
		}
		return handle;
	}

	EntityHandle Scene::CreateLightEntity(const std::string& name, LightType type)
	{
		const EntityHandle handle = CreateEntity(name);
		if (Entity* entity = TryGetEntity(handle))
			entity->AddComponent<LightComponent>(LightComponent{ Light{ type } });
		return handle;
	}

	void Scene::OnEntityDestroyed(EntityHandle entityHandle)
	{
		auto entity = m_SceneRegistry->EntityPool.Get(entityHandle);
		if (!entity || !m_SceneRegistry->Registry.valid(entity->GetEnttHandle()))
			return;

		const entt::entity handle = entity->GetEnttHandle();
		auto& Registry = m_SceneRegistry->Registry;
		if (Registry.all_of<TagComponent>(handle)) {
			const auto id = Registry.get<TagComponent>(handle).ID;
			m_SceneRegistry->EntityHandlersStore.erase(id);
		}
		m_SceneRegistry->EntityPool.Remove(entityHandle);
		m_SceneRegistry->Registry.destroy(handle);
	}

	void Scene::DestroyEntity(EntityHandle entity)
	{
		OnEntityDestroyed(entity);
	}

	EntityHandle Scene::TryGetEntityHandleByID(TagComponentIDType id) const
	{
		auto it = m_SceneRegistry->EntityHandlersStore.find(id);
		if (it == m_SceneRegistry->EntityHandlersStore.end())
			return {};

		Entity* pooledEntity = m_SceneRegistry->EntityPool.Get(it->second);
		if (!pooledEntity) {
			return {};
		}

		if (!IsEnttEntityValid(pooledEntity->GetEnttHandle())) {
			return {};
		}

		return it->second;
	}

	TagComponentIDType Scene::GetID(EntityHandle entityHandle) const
	{
		auto* entity = m_SceneRegistry->EntityPool.Get(entityHandle);
		if (!entity)
			return 0;

		const entt::entity handle = entity->GetEnttHandle();
		const auto& Registry = m_SceneRegistry->Registry;
		if (!Registry.all_of<TagComponent>(handle))
			return 0;
		return Registry.get<TagComponent>(handle).ID;
	}

	Entity* Scene::TryGetEntity(EntityHandle entityHandle)
	{
		Entity* entity = m_SceneRegistry->EntityPool.Get(entityHandle);
		if (!entity || !IsEnttEntityValid(entity->GetEnttHandle()))
			return nullptr;
		return entity;
	}

	const Entity* Scene::TryGetEntity(EntityHandle entityHandle) const
	{
		Entity* entity = m_SceneRegistry->EntityPool.Get(entityHandle);
		if (!entity || !IsEnttEntityValid(entity->GetEnttHandle()))
			return nullptr;
		return entity;
	}

	bool Scene::IsEntityHandleValid(EntityHandle entityHandle) const
	{
		return TryGetEntity(entityHandle) != nullptr;
	}

	void Scene::SetLightComponent(EntityHandle entityHandle, const Light& light)
	{
		if (auto* e = TryGetEntity(entityHandle)) {
			if (e->HasComponent<LightComponent>())
				e->RemoveComponent<LightComponent>();
			e->AddComponent<LightComponent>(LightComponent{ light });
		}
	}

	void Scene::RemoveLightComponent(EntityHandle entityHandle)
	{
		if (auto* e = TryGetEntity(entityHandle); e && e->HasComponent<LightComponent>())
			e->RemoveComponent<LightComponent>();
	}

	void Scene::RemoveObject(TagComponentIDType id)
	{
		DestroyEntity(TryGetEntityHandleByID(id));
	}

	void Scene::Clear()
	{
		m_SceneRegistry->EntityHandlersStore.clear();
		m_SceneRegistry->EntityPool.Clear();
		m_SceneRegistry->Registry.clear();
		m_SceneRegistry->NextID = 1;
	}

	std::string Scene::GetMeshSourcePath(TagComponentIDType objectID) const
	{
		auto view = m_SceneRegistry->Registry.view<TagComponent, MeshComponent>();
		for (auto e : view) {
			if (view.get<TagComponent>(e).ID == objectID)
				return view.get<MeshComponent>(e).SourcePath;
		}
		return {};
	}

	bool Scene::IsEnttEntityValid(entt::entity entity) const
	{
		return entity != entt::null && m_SceneRegistry->Registry.valid(entity);
	}

	template<typename T>
	T* Scene::TryGetComponent(EntityHandle entityHandle)
	{
		Entity* e = TryGetEntity(entityHandle);
		if (!e)
			return nullptr;
		const entt::entity h = e->GetEnttHandle();
		if (!HasComponent<T>(h))
			return nullptr;
		return &GetComponent<T>(h);
	}

	template<typename T>
	const T* Scene::TryGetComponent(EntityHandle entityHandle) const
	{
		return const_cast<Scene*>(this)->TryGetComponent<T>(entityHandle);
	}

	template<typename T, typename... Args>
	T& Scene::AddComponent(entt::entity enttHandler, Args&&... args)
	{
		CHE_CORE_ASSERT(!HasComponent<T>(enttHandler), "Entity already has this component");
		return m_SceneRegistry->Registry.emplace<T>(enttHandler, std::forward<Args>(args)...);
	}

	template<typename T>
	T& Scene::GetComponent(entt::entity enttHandler)
	{
		CHE_CORE_ASSERT(HasComponent<T>(enttHandler), "Entity does not have this component");
		return m_SceneRegistry->Registry.get<T>(enttHandler);
	}

	template<typename T>
	const T& Scene::GetComponent(entt::entity enttHandler) const
	{
		CHE_CORE_ASSERT(HasComponent<T>(enttHandler), "Entity does not have this component");
		return m_SceneRegistry->Registry.get<T>(enttHandler);
	}

	template<typename T>
	bool Scene::HasComponent(entt::entity enttHandler) const
	{
		return m_SceneRegistry->Registry.all_of<T>(enttHandler);
	}

	template<typename T>
	void Scene::RemoveComponent(entt::entity enttHandler)
	{
		CHE_CORE_ASSERT(HasComponent<T>(enttHandler), "Entity does not have this component");
		m_SceneRegistry->Registry.remove<T>(enttHandler);
	}

	template TagComponent& Scene::AddComponent<TagComponent>(entt::entity, TagComponent&&);
	template TransformComponent& Scene::AddComponent<TransformComponent>(entt::entity);
	template MeshComponent& Scene::AddComponent<MeshComponent>(entt::entity);
	template ColorComponent& Scene::AddComponent<ColorComponent>(entt::entity);
	template VisibilityComponent& Scene::AddComponent<VisibilityComponent>(entt::entity);
	template LightComponent& Scene::AddComponent<LightComponent>(entt::entity, LightComponent&&);

	template TagComponent& Scene::GetComponent<TagComponent>(entt::entity);
	template TransformComponent& Scene::GetComponent<TransformComponent>(entt::entity);
	template MeshComponent& Scene::GetComponent<MeshComponent>(entt::entity);
	template ColorComponent& Scene::GetComponent<ColorComponent>(entt::entity);
	template VisibilityComponent& Scene::GetComponent<VisibilityComponent>(entt::entity);
	template LightComponent& Scene::GetComponent<LightComponent>(entt::entity);
	template const TagComponent& Scene::GetComponent<TagComponent>(entt::entity) const;
	template const TransformComponent& Scene::GetComponent<TransformComponent>(entt::entity) const;
	template const MeshComponent& Scene::GetComponent<MeshComponent>(entt::entity) const;
	template const ColorComponent& Scene::GetComponent<ColorComponent>(entt::entity) const;
	template const VisibilityComponent& Scene::GetComponent<VisibilityComponent>(entt::entity) const;
	template const LightComponent& Scene::GetComponent<LightComponent>(entt::entity) const;

	template bool Scene::HasComponent<TagComponent>(entt::entity) const;
	template bool Scene::HasComponent<TransformComponent>(entt::entity) const;
	template bool Scene::HasComponent<MeshComponent>(entt::entity) const;
	template bool Scene::HasComponent<ColorComponent>(entt::entity) const;
	template bool Scene::HasComponent<VisibilityComponent>(entt::entity) const;
	template bool Scene::HasComponent<LightComponent>(entt::entity) const;

	template void Scene::RemoveComponent<TagComponent>(entt::entity);
	template void Scene::RemoveComponent<TransformComponent>(entt::entity);
	template void Scene::RemoveComponent<MeshComponent>(entt::entity);
	template void Scene::RemoveComponent<ColorComponent>(entt::entity);
	template void Scene::RemoveComponent<VisibilityComponent>(entt::entity);
	template void Scene::RemoveComponent<LightComponent>(entt::entity);

	template CHENGINE_API TagComponent* Scene::TryGetComponent<TagComponent>(EntityHandle);
	template CHENGINE_API TransformComponent* Scene::TryGetComponent<TransformComponent>(EntityHandle);
	template CHENGINE_API MeshComponent* Scene::TryGetComponent<MeshComponent>(EntityHandle);
	template CHENGINE_API ColorComponent* Scene::TryGetComponent<ColorComponent>(EntityHandle);
	template CHENGINE_API VisibilityComponent* Scene::TryGetComponent<VisibilityComponent>(EntityHandle);
	template CHENGINE_API LightComponent* Scene::TryGetComponent<LightComponent>(EntityHandle);

	template CHENGINE_API const TagComponent* Scene::TryGetComponent<TagComponent>(EntityHandle) const;
	template CHENGINE_API const TransformComponent* Scene::TryGetComponent<TransformComponent>(EntityHandle) const;
	template CHENGINE_API const MeshComponent* Scene::TryGetComponent<MeshComponent>(EntityHandle) const;
	template CHENGINE_API const ColorComponent* Scene::TryGetComponent<ColorComponent>(EntityHandle) const;
	template CHENGINE_API const VisibilityComponent* Scene::TryGetComponent<VisibilityComponent>(EntityHandle) const;
	template CHENGINE_API const LightComponent* Scene::TryGetComponent<LightComponent>(EntityHandle) const;

} // namespace CHEngine
