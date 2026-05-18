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

		void CopyMeshComponent(const MeshComponent& source_component, MeshComponent& destination_component)
		{
			destination_component.SourcePath = source_component.SourcePath;
			destination_component.Meshes = source_component.Meshes;
		}

		void CopyRigidBody3DComponent(const RigidBody3DComponent& source_component, RigidBody3DComponent& destination_component)
		{
			destination_component.BodyDesc = source_component.BodyDesc;
			destination_component.ShapeDesc = source_component.ShapeDesc;
			destination_component.SyncMode = source_component.SyncMode;
			destination_component.Body = nullptr;
			destination_component.Shape = nullptr;
		}

		template<typename ComponentType>
		void CopySingleComponentType(entt::registry& destination_registry,
		                         const entt::registry& source_registry,
		                         const entt::entity destination_entity,
		                         const entt::entity source_entity)
		{
			if (!source_registry.all_of<ComponentType>(source_entity))
			{
				destination_registry.remove<ComponentType>(destination_entity);
				return;
			}

			if (!destination_registry.all_of<ComponentType>(destination_entity))
				destination_registry.emplace<ComponentType>(destination_entity);

			if constexpr (std::is_same_v<ComponentType, MeshComponent>)
			{
				CopyMeshComponent(source_registry.get<MeshComponent>(source_entity),
				                  destination_registry.get<MeshComponent>(destination_entity));
			}
			else if constexpr (std::is_same_v<ComponentType, RigidBody3DComponent>)
			{
				CopyRigidBody3DComponent(source_registry.get<RigidBody3DComponent>(source_entity),
				                         destination_registry.get<RigidBody3DComponent>(destination_entity));
			}
			else
			{
				destination_registry.get<ComponentType>(destination_entity)
					= source_registry.get<ComponentType>(source_entity);
			}
		}

		template<typename... Components>
		void CopyComponents(ComponentGroup<Components...>,
		                    entt::registry& destination_registry,
		                    const entt::registry& source_registry,
		                    const entt::entity destination_entity,
		                    const entt::entity source_entity)
		{
			(CopySingleComponentType<Components>(
				 destination_registry,
				 source_registry,
				 destination_entity,
				 source_entity),
			 ...);
		}
	} // namespace

	Scene::Scene()
	{
		InitializeRegistry();
		InitSignals();
	}

	Scene::Scene(const Scene& other)
		:Scene()
	{
		CloneFrom(other);
	}

	Scene::~Scene() = default;

	Scene::Scene(Scene&& other)
		: m_SceneRegistry(std::move(other.m_SceneRegistry))
	{
		if (!m_SceneRegistry)
		{
			InitializeRegistry();
		}

		m_SceneRegistry->EntityPool.ForEachOccupied([this](Entity* entity) { entity->SetScene(this); });

		other.InitializeRegistry();
		other.InitSignals();
	}

	Scene& Scene::operator=(const Scene& other)
	{
		if (this == &other)
			return *this;

		InitializeRegistry();
		InitSignals();
		CloneFrom(other);
		return *this;
	}

	Scene& Scene::operator=(Scene&& other)
	{
		if (this == &other)
			return *this;

		m_SceneRegistry = std::move(other.m_SceneRegistry);
		if (!m_SceneRegistry)
		{
			InitializeRegistry();
		}

		m_SceneRegistry->EntityPool.ForEachOccupied([this](Entity* entity) { entity->SetScene(this); });

		// The moved registry's on_update signal still points to 'other' — rewire it to 'this'
		// so it doesn't become a dangling pointer when 'other' (the temporary loadedScene) is
		// destroyed after the assignment.
		InitSignals();

		other.InitializeRegistry();
		other.InitSignals();

		return *this;
	}

	void Scene::InitializeRegistry()
	{
		m_SceneRegistry = std::make_unique<SceneRegistry>();
		m_SceneRegistry->EntityPool = HandlePool<Entity, EntityTag>([](Entity* ptr) { delete ptr; });
	}

	void Scene::InitSignals()
	{
		auto& reg = m_SceneRegistry->Registry;
		// Disconnect before connecting so this is safe to call on a registry whose
		// signal handlers still point to a different Scene instance (e.g. after move).
		reg.on_update<TransformComponent>().disconnect();
		reg.on_update<TransformComponent>().connect<&Scene::OnTransformUpdate>(this);
	}

	void Scene::CloneFrom(const Scene& source)
	{
		auto& sourceRegistry = source.m_SceneRegistry->Registry;
		auto& destinationRegistry = m_SceneRegistry->Registry;

		auto sourceView = sourceRegistry.view<IDComponent, TagComponent>();
		for (entt::entity sourceEntt : sourceView)
		{
			const auto& sourceId = sourceRegistry.get<IDComponent>(sourceEntt);
			const auto& sourceTag = sourceRegistry.get<TagComponent>(sourceEntt);
			CreateEntity(sourceTag.Name, sourceId.Value);
		}

		for (entt::entity sourceEntt : sourceView)
		{
			const auto& sourceId = sourceRegistry.get<IDComponent>(sourceEntt).Value;
			const EntityHandle destinationHandle = TryGetEntityHandleByUUID(sourceId);
			CHE_CORE_ASSERT(destinationHandle.IsValid(), "Scene copy failed: destination entity not found");

			const entt::entity destinationEntt = TryGetEnttEntity(destinationHandle);
			CHE_CORE_ASSERT(destinationEntt != entt::null, "Scene copy failed: destination entt is null");

			CopyComponents(CopyableSceneComponents{}, destinationRegistry, sourceRegistry, destinationEntt, sourceEntt);
		}

		WorldScripts = source.WorldScripts;
	}

	EntityHandle Scene::CreateEntity(const std::string& name, const UUID& uuid)
	{
		CHE_CORE_ASSERT(m_SceneRegistry->EntityHandlersStore.find(uuid) == m_SceneRegistry->EntityHandlersStore.end(),
		                "CreateEntity: duplicate entity UUID");
		const entt::entity enttEntity = m_SceneRegistry->Registry.create();
		auto* entity = new Entity{ enttEntity, this };

		entity->AddComponent<TagComponent>(TagComponent{ name });
		entity->AddComponent<IDComponent>(IDComponent{ uuid });
		entity->AddComponent<VisibilityComponent>();

		const EntityHandle handle = m_SceneRegistry->EntityPool.Add(entity);
		m_SceneRegistry->EntityHandlersStore[uuid] = handle;
		m_SceneRegistry->EnttToHandleStore[enttEntity] = handle;

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
		m_SceneRegistry->EnttToHandleStore.erase(handle);
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

	const UUID Scene::GetUUID(const EntityHandle entityHandle) const
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
		m_SceneRegistry->EnttToHandleStore.clear();
		m_SceneRegistry->EntityPool.Clear();
		m_SceneRegistry->Registry.clear();
	}

	EntityHandle Scene::TryGetEntityHandleByEntt(entt::entity e) const
	{
		const auto it = m_SceneRegistry->EnttToHandleStore.find(e);
		if (it == m_SceneRegistry->EnttToHandleStore.end())
			return {};
		return it->second;
	}

	namespace
	{
		PhysicsColliderShapeDesc BuildScaledShapeDesc(const PhysicsColliderShapeDesc& base_shape_desc, const Transform& transform)
		{
			PhysicsColliderShapeDesc scaledShapeDesc = base_shape_desc;
			const glm::vec3 absScale = glm::abs(transform.Scale);
			const float maxAxisAbsScale = std::max(absScale.x, std::max(absScale.y, absScale.z));

			switch (scaledShapeDesc.Type)
			{
			case PhysicsColliderShapeType::Box:
				scaledShapeDesc.HalfExtents = absScale;
				break;
			case PhysicsColliderShapeType::Sphere:
				scaledShapeDesc.Radius = maxAxisAbsScale;
				break;
			case PhysicsColliderShapeType::Capsule:
				scaledShapeDesc.Radius = maxAxisAbsScale;
				scaledShapeDesc.HalfHeight = maxAxisAbsScale;
				break;
			default:
				break;
			}

			return scaledShapeDesc;
		}
	}

	// On Signals
	void Scene::OnTransformUpdate(entt::registry& reg, entt::entity e)
	{
		if (!reg.all_of<TransformComponent, RigidBody3DComponent>(e)) return;

		auto& tc = reg.get<TransformComponent>(e);
		auto& rb = reg.get<RigidBody3DComponent>(e);

		if (rb.SynchronisedTransform)
		{
			PhysicsTransform physTransform;
			physTransform.Position = tc.ObjectTransform.Position;
			physTransform.Rotation = glm::quat(glm::radians(tc.ObjectTransform.Rotation));

			const auto scaledDesc = BuildScaledShapeDesc(rb.ShapeDesc, tc.ObjectTransform);
			
			rb.ShapeDesc = scaledDesc;
			rb.BodyDesc.Transform = physTransform;
		}
	}
} // namespace CHEngine
