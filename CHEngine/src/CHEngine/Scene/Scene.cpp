#include "chepch.h"
#include "Scene.h"
#include "Entity.h"
#include "Components.h"

#include <entt/entt.hpp>
#include <random>

namespace CHEngine {

	namespace {
	UUID GenerateEntityUUID()
	{
		static thread_local std::mt19937 rng(std::random_device{}());
		static thread_local std::uniform_int_distribution<int> dist(0, 255);

		UUID uuid = boost::uuids::nil_uuid();
		for (auto& byte : uuid)
			byte = static_cast<uint8_t>(dist(rng));

		// RFC 4122 version 4 (random) + variant bits.
		auto it = uuid.begin();
		*(it + 6) = static_cast<uint8_t>((*(it + 6) & 0x0F) | 0x40);
		*(it + 8) = static_cast<uint8_t>((*(it + 8) & 0x3F) | 0x80);
		return uuid;
	}
	} // namespace

	Scene::Scene()
		: m_SceneRegistry(std::make_unique<SceneRegistry>())
	{
		m_SceneRegistry->EntityPool = HandlePool<Entity, EntityTag>([](Entity* ptr) { delete ptr; });
	}

	Scene::~Scene() = default;

	Scene::Scene(Scene&&) = default;
	Scene& Scene::operator=(Scene&&) = default;

	EntityHandle Scene::CreateEntity(const std::string& name, const UUID& uuid)
	{
		CHE_CORE_ASSERT(m_SceneRegistry->EntityHandlersStore.find(uuid) == m_SceneRegistry->EntityHandlersStore.end(),
		                "CreateEntity: duplicate entity UUID");
		auto* entity = new Entity{ m_SceneRegistry->Registry.create(), this };

		entity->AddComponent<TagComponent>(TagComponent{ name });
		entity->AddComponent<IDComponent>(IDComponent{ uuid });
		entity->AddComponent<TransformComponent>();
		entity->AddComponent<MeshComponent>();
		entity->AddComponent<ColorComponent>();
		entity->AddComponent<VisibilityComponent>();

		const EntityHandle handle = m_SceneRegistry->EntityPool.Add(entity);
		m_SceneRegistry->EntityHandlersStore[uuid] = handle;

		return handle;
	}

	EntityHandle Scene::CreateEntity(const std::string& name)
	{
		return CreateEntity(name, GenerateEntityUUID());
	}

	void Scene::DestroyEntity(EntityHandle entityHandle)
	{
		auto entity = m_SceneRegistry->EntityPool.Get(entityHandle);
		if (!entity || !m_SceneRegistry->Registry.valid(entity->GetEnttHandle()))
			return;

		const entt::entity handle = entity->GetEnttHandle();
		auto& Registry = m_SceneRegistry->Registry;
		if (Registry.all_of<IDComponent>(handle)) {
			const UUID& uuid = Registry.get<IDComponent>(handle).Value;
			m_SceneRegistry->EntityHandlersStore.erase(uuid);
		}
		m_SceneRegistry->EntityPool.Remove(entityHandle);
		m_SceneRegistry->Registry.destroy(handle);
	}

	EntityHandle Scene::TryGetEntityHandleByUUID(const UUID& uuid) const
	{
		auto it = m_SceneRegistry->EntityHandlersStore.find(uuid);
		if (it == m_SceneRegistry->EntityHandlersStore.end())
			return {};

		const Entity* pooledEntity = m_SceneRegistry->EntityPool.Get(it->second);
		if (!pooledEntity || !pooledEntity->IsValid())
			return {};

		return it->second;
	}

	UUID Scene::GetUUID(const EntityHandle entityHandle) const
	{
		auto* entity = m_SceneRegistry->EntityPool.Get(entityHandle);
		if (!entity)
			return boost::uuids::nil_uuid();

		const entt::entity handle = entity->GetEnttHandle();
		const auto& Registry = m_SceneRegistry->Registry;
		if (!Registry.all_of<IDComponent>(handle))
			return boost::uuids::nil_uuid();
		return Registry.get<IDComponent>(handle).Value;
	}

	Entity* Scene::TryGetEntity(const EntityHandle entityHandle)
	{
		Entity* entity = m_SceneRegistry->EntityPool.Get(entityHandle);
		if (!entity || !entity->IsValid() || !m_SceneRegistry->Registry.valid(entity->GetEnttHandle()))
			return nullptr;
		return entity;
	}

	const Entity* Scene::TryGetEntity(const EntityHandle entityHandle) const
	{
		const Entity* entity = m_SceneRegistry->EntityPool.Get(entityHandle);
		if (!entity || !entity->IsValid() || !m_SceneRegistry->Registry.valid(entity->GetEnttHandle()))
			return nullptr;
		return entity;
	}

	bool Scene::IsEntityHandleValid(const EntityHandle entityHandle) const
	{
		return TryGetEntity(entityHandle) != nullptr;
	}

	entt::entity Scene::TryGetEnttEntity(const EntityHandle entityHandle) const
	{
		const Entity* entity = TryGetEntity(entityHandle);
		if (!entity)
			return entt::null;
		return entity->GetEnttHandle();
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

	void Scene::RemoveObject(const UUID& uuid)
	{
		DestroyEntity(TryGetEntityHandleByUUID(uuid));
	}

	void Scene::Clear()
	{
		m_SceneRegistry->EntityHandlersStore.clear();
		m_SceneRegistry->EntityPool.Clear();
		m_SceneRegistry->Registry.clear();
	}

	std::string Scene::GetMeshSourcePath(const UUID& objectUUID) const
	{
		auto view = m_SceneRegistry->Registry.view<IDComponent, MeshComponent>();
		for (auto e : view) {
			if (view.get<IDComponent>(e).Value == objectUUID)
				return view.get<MeshComponent>(e).SourcePath;
		}
		return {};
	}

	//template<typename T>
	//T* Scene::TryGetComponent(EntityHandle entityHandle)
	//{
	//	Entity* e = TryGetEntity(entityHandle);
	//	if (!e)
	//		return nullptr;
	//	const entt::entity h = e->GetEnttHandle();
	//	if (!HasComponent<T>(h))
	//		return nullptr;
	//	return &GetComponent<T>(h);
	//}

	//template CHENGINE_API TagComponent& Scene::AddComponent<TagComponent>(entt::entity, TagComponent&&);
	//template CHENGINE_API TransformComponent& Scene::AddComponent<TransformComponent>(entt::entity);
	//template CHENGINE_API MeshComponent& Scene::AddComponent<MeshComponent>(entt::entity);
	//template CHENGINE_API ColorComponent& Scene::AddComponent<ColorComponent>(entt::entity);
	//template CHENGINE_API VisibilityComponent& Scene::AddComponent<VisibilityComponent>(entt::entity);
	//template CHENGINE_API LightComponent& Scene::AddComponent<LightComponent>(entt::entity, LightComponent&&);
	//template CHENGINE_API RigidBody3DComponent& Scene::AddComponent<RigidBody3DComponent>(entt::entity, RigidBody3DComponent&&);
	//template CHENGINE_API TransformDirtyComponent& Scene::AddComponent<TransformDirtyComponent>(entt::entity, TransformDirtyComponent&&);
	//template CHENGINE_API LifetimeComponent& Scene::AddComponent<LifetimeComponent>(entt::entity, LifetimeComponent&&);
	//template CHENGINE_API CameraComponent& Scene::AddComponent<CameraComponent, CameraComponent>(entt::entity, CameraComponent&&);
	//		 
	//template CHENGINE_API TagComponent& Scene::GetComponent<TagComponent>(entt::entity);
	//template CHENGINE_API TransformComponent& Scene::GetComponent<TransformComponent>(entt::entity);
	//template CHENGINE_API MeshComponent& Scene::GetComponent<MeshComponent>(entt::entity);
	//template CHENGINE_API ColorComponent& Scene::GetComponent<ColorComponent>(entt::entity);
	//template CHENGINE_API VisibilityComponent& Scene::GetComponent<VisibilityComponent>(entt::entity);
	//template CHENGINE_API LightComponent& Scene::GetComponent<LightComponent>(entt::entity);
	//template CHENGINE_API RigidBody3DComponent& Scene::GetComponent<RigidBody3DComponent>(entt::entity);
	//template CHENGINE_API TransformDirtyComponent& Scene::GetComponent<TransformDirtyComponent>(entt::entity);
	//template CHENGINE_API LifetimeComponent& Scene::GetComponent<LifetimeComponent>(entt::entity);
	//template CHENGINE_API CameraComponent& Scene::GetComponent<CameraComponent>(entt::entity);
	//template CHENGINE_API const TagComponent& Scene::GetComponent<TagComponent>(entt::entity) const;
	//template CHENGINE_API const TransformComponent& Scene::GetComponent<TransformComponent>(entt::entity) const;
	//template CHENGINE_API const MeshComponent& Scene::GetComponent<MeshComponent>(entt::entity) const;
	//template CHENGINE_API const ColorComponent& Scene::GetComponent<ColorComponent>(entt::entity) const;
	//template CHENGINE_API const VisibilityComponent& Scene::GetComponent<VisibilityComponent>(entt::entity) const;
	//template CHENGINE_API const LightComponent& Scene::GetComponent<LightComponent>(entt::entity) const;
	//template CHENGINE_API const RigidBody3DComponent& Scene::GetComponent<RigidBody3DComponent>(entt::entity) const;
	//template CHENGINE_API const TransformDirtyComponent& Scene::GetComponent<TransformDirtyComponent>(entt::entity) const;
	//template CHENGINE_API const LifetimeComponent& Scene::GetComponent<LifetimeComponent>(entt::entity) const;
	//template CHENGINE_API const CameraComponent& Scene::GetComponent<CameraComponent>(entt::entity) const;
	//		 
	//template CHENGINE_API bool Scene::HasComponent<TagComponent>(entt::entity) const;
	//template CHENGINE_API bool Scene::HasComponent<TransformComponent>(entt::entity) const;
	//template CHENGINE_API bool Scene::HasComponent<MeshComponent>(entt::entity) const;
	//template CHENGINE_API bool Scene::HasComponent<ColorComponent>(entt::entity) const;
	//template CHENGINE_API bool Scene::HasComponent<VisibilityComponent>(entt::entity) const;
	//template CHENGINE_API bool Scene::HasComponent<LightComponent>(entt::entity) const;
	//template CHENGINE_API bool Scene::HasComponent<RigidBody3DComponent>(entt::entity) const;
	//template CHENGINE_API bool Scene::HasComponent<TransformDirtyComponent>(entt::entity) const;
	//template CHENGINE_API bool Scene::HasComponent<LifetimeComponent>(entt::entity) const;
	//template CHENGINE_API bool Scene::HasComponent<CameraComponent>(entt::entity) const;
	//		 
	//template CHENGINE_API void Scene::RemoveComponent<TagComponent>(entt::entity);
	//template CHENGINE_API void Scene::RemoveComponent<TransformComponent>(entt::entity);
	//template CHENGINE_API void Scene::RemoveComponent<MeshComponent>(entt::entity);
	//template CHENGINE_API void Scene::RemoveComponent<ColorComponent>(entt::entity);
	//template CHENGINE_API void Scene::RemoveComponent<VisibilityComponent>(entt::entity);
	//template CHENGINE_API void Scene::RemoveComponent<LightComponent>(entt::entity);
	//template CHENGINE_API void Scene::RemoveComponent<RigidBody3DComponent>(entt::entity);
	//template CHENGINE_API void Scene::RemoveComponent<TransformDirtyComponent>(entt::entity);
	//template CHENGINE_API void Scene::RemoveComponent<LifetimeComponent>(entt::entity);
	//template CHENGINE_API void Scene::RemoveComponent<CameraComponent>(entt::entity);

	//template CHENGINE_API TagComponent* Scene::TryGetComponent<TagComponent>(EntityHandle);
	//template CHENGINE_API IDComponent* Scene::TryGetComponent<IDComponent>(EntityHandle);
	//template CHENGINE_API TransformComponent* Scene::TryGetComponent<TransformComponent>(EntityHandle);
	//template CHENGINE_API MeshComponent* Scene::TryGetComponent<MeshComponent>(EntityHandle);
	//template CHENGINE_API ColorComponent* Scene::TryGetComponent<ColorComponent>(EntityHandle);
	//template CHENGINE_API VisibilityComponent* Scene::TryGetComponent<VisibilityComponent>(EntityHandle);
	//template CHENGINE_API LightComponent* Scene::TryGetComponent<LightComponent>(EntityHandle);
	//template CHENGINE_API RigidBody3DComponent* Scene::TryGetComponent<RigidBody3DComponent>(EntityHandle);
	//template CHENGINE_API TransformDirtyComponent* Scene::TryGetComponent<TransformDirtyComponent>(EntityHandle);
	//template CHENGINE_API LifetimeComponent* Scene::TryGetComponent<LifetimeComponent>(EntityHandle);
	//template CHENGINE_API CameraComponent* Scene::TryGetComponent<CameraComponent>(EntityHandle);

	//template CHENGINE_API const TagComponent* Scene::TryGetComponent<TagComponent>(EntityHandle) const;
	//template CHENGINE_API const IDComponent* Scene::TryGetComponent<IDComponent>(EntityHandle) const;
	//template CHENGINE_API const TransformComponent* Scene::TryGetComponent<TransformComponent>(EntityHandle) const;
	//template CHENGINE_API const MeshComponent* Scene::TryGetComponent<MeshComponent>(EntityHandle) const;
	//template CHENGINE_API const ColorComponent* Scene::TryGetComponent<ColorComponent>(EntityHandle) const;
	//template CHENGINE_API const VisibilityComponent* Scene::TryGetComponent<VisibilityComponent>(EntityHandle) const;
	//template CHENGINE_API const LightComponent* Scene::TryGetComponent<LightComponent>(EntityHandle) const;
	//template CHENGINE_API const RigidBody3DComponent* Scene::TryGetComponent<RigidBody3DComponent>(EntityHandle) const;
	//template CHENGINE_API const TransformDirtyComponent* Scene::TryGetComponent<TransformDirtyComponent>(EntityHandle) const;
	//template CHENGINE_API const LifetimeComponent* Scene::TryGetComponent<LifetimeComponent>(EntityHandle) const;
	//template CHENGINE_API const CameraComponent* Scene::TryGetComponent<CameraComponent>(EntityHandle) const;

} // namespace CHEngine
