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
		using namespace entt::literals;

		auto& srcReg = source.m_SceneRegistry->Registry;
		auto& dstReg = m_SceneRegistry->Registry;

		// 1. Create all entities preserving UUIDs and names.
		auto srcView = srcReg.view<IDComponent, TagComponent>();
		for (entt::entity srcEntt : srcView)
		{
			const auto& id  = srcReg.get<IDComponent>(srcEntt);
			const auto& tag = srcReg.get<TagComponent>(srcEntt);
			CreateEntity(tag.Name, id.Value);
		}

		// 2. Copy all registered components via entt::meta clone_to.
		for (entt::entity srcEntt : srcView)
		{
			const UUID uuid = srcReg.get<IDComponent>(srcEntt).Value;
			const EntityHandle dstHandle = TryGetEntityHandleByUUID(uuid);
			CHE_CORE_ASSERT(dstHandle.IsValid(), "Scene copy failed: destination entity not found");

			const entt::entity dstEntt = TryGetEnttEntity(dstHandle);
			CHE_CORE_ASSERT(dstEntt != entt::null, "Scene copy failed: destination entt is null");

			for (auto&& [typeId, storage] : srcReg.storage())
			{
				if (!storage.contains(srcEntt)) continue;

				const auto& typeInfo = storage.type();
				// IDComponent / TagComponent are already set by CreateEntity.
				if (typeInfo == entt::type_id<IDComponent>() ||
				    typeInfo == entt::type_id<TagComponent>())
					continue;

				auto metaType = entt::resolve(typeInfo);
				if (!metaType) continue;
				auto cloneFn = metaType.func("clone_to"_hs);
				if (!cloneFn) continue;
				// src is logically const here; CloneTo only reads from it.
				cloneFn.invoke({},
				    entt::forward_as_meta(dstReg),
				    entt::forward_as_meta(dstEntt),
				    entt::forward_as_meta(const_cast<entt::registry&>(srcReg)),
				    entt::forward_as_meta(srcEntt));
			}
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
