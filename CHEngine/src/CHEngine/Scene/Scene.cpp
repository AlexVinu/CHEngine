#include "chepch.h"
#include "Scene.h"
#include "Entity.h"
#include "Components.h"

#include <entt/entt.hpp>
#include <boost/uuid/random_generator.hpp>

namespace CHEngine {

	namespace {
		UUID GenerateEntityUUID()
		{
			return boost::uuids::random_generator()();
		}
	} // namespace

	Scene::Scene()
		: m_SceneRegistry(std::make_unique<SceneRegistry>())
	{
		m_SceneRegistry->EntityPool = HandlePool<Entity, EntityTag>([](Entity* ptr) { delete ptr; });
	}

	Scene::~Scene() = default;

	Scene::Scene(Scene&& other)
		: m_SceneRegistry(std::move(other.m_SceneRegistry))
	{
		if (!m_SceneRegistry)
		{
			m_SceneRegistry = std::make_unique<SceneRegistry>();
			m_SceneRegistry->EntityPool = HandlePool<Entity, EntityTag>([](Entity* ptr) { delete ptr; });
		}

		m_SceneRegistry->EntityPool.ForEachOccupied([this](Entity* entity) { entity->SetScene(this); });

		other.m_SceneRegistry = std::make_unique<SceneRegistry>();
		other.m_SceneRegistry->EntityPool = HandlePool<Entity, EntityTag>([](Entity* ptr) { delete ptr; });
	}

	Scene& Scene::operator=(Scene&& other)
	{
		if (this == &other)
			return *this;

		m_SceneRegistry = std::move(other.m_SceneRegistry);
		if (!m_SceneRegistry)
		{
			m_SceneRegistry = std::make_unique<SceneRegistry>();
			m_SceneRegistry->EntityPool = HandlePool<Entity, EntityTag>([](Entity* ptr) { delete ptr; });
		}

		m_SceneRegistry->EntityPool.ForEachOccupied([this](Entity* entity) { entity->SetScene(this); });

		other.m_SceneRegistry = std::make_unique<SceneRegistry>();
		other.m_SceneRegistry->EntityPool = HandlePool<Entity, EntityTag>([](Entity* ptr) { delete ptr; });

		return *this;
	}

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

	void Scene::DestroyEntity(const UUID& uuid)
	{
		DestroyEntity(TryGetEntityHandleByUUID(uuid));
	}

	void Scene::Clear()
	{
		m_SceneRegistry->EntityHandlersStore.clear();
		m_SceneRegistry->EntityPool.Clear();
		m_SceneRegistry->Registry.clear();
	}

} // namespace CHEngine
