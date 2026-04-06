#include "chepch.h"
#include "Scene.h"
#include "Entity.h"
#include "Components.h"
#include "CHEngine/Physics/PhysicsFacade.h"

#include <entt/entt.hpp>
#include <glm/gtc/quaternion.hpp>
#include <random>
#include <type_traits>

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
		PhysicsFacade::RegisterScene(this);
		InitPhysicsWorld();
	}

	Scene::~Scene()
	{
		if(m_PhysicsWorld)
			PhysicsFacade::UnregisterScene(this);
	}

	Scene::Scene(Scene&&) = default;
	Scene& Scene::operator=(Scene&&) = default;

	EntityHandle Scene::CreateEntity(const std::string& name, const UUID& uuid)
	{
		CHE_CORE_ASSERT(m_SceneRegistry->EntityHandlersStore.find(uuid) == m_SceneRegistry->EntityHandlersStore.end(),
		                "CreateEntity: duplicate entity UUID");
		auto* entity = new Entity{ m_SceneRegistry->Registry.create(), this };
		const EntityHandle handle = m_SceneRegistry->EntityPool.Add(entity);
		entity->AddComponent<TagComponent>(TagComponent{ name });
		entity->AddComponent<IDComponent>(IDComponent{ uuid });
		entity->AddComponent<TransformComponent>();
		entity->AddComponent<MeshComponent>();
		entity->AddComponent<ColorComponent>();
		entity->AddComponent<VisibilityComponent>();
		m_SceneRegistry->EntityHandlersStore[uuid] = handle;

		return handle;
	}

	EntityHandle Scene::CreateEntity(const std::string& name)
	{
		return CreateEntity(name, GenerateEntityUUID());
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

	EntityHandle Scene::CreatePhysicsEntity(const std::string& name,
	                                        const PhysicsRigidBodyDesc& bodyDesc,
	                                        const IPhysicsShape* shape,
	                                        RigidBodySyncMode syncMode)
	{
		const EntityHandle handle = CreateEntity(name);
		if (!AttachRigidBody(handle, bodyDesc, shape, syncMode))
			CHE_CORE_WARN("Scene::CreatePhysicsEntity: failed to attach rigid body for '{}'", name);
		return handle;
	}

	IPhysicsBody* Scene::AttachRigidBody(EntityHandle entityHandle,
	                                     const PhysicsRigidBodyDesc& bodyDesc,
	                                     const IPhysicsShape* shape,
	                                     RigidBodySyncMode syncMode)
	{
		Entity* entity = TryGetEntity(entityHandle);
		if (!entity || !shape || !m_PhysicsWorld)
			return nullptr;

		const entt::entity enttHandle = entity->GetEnttHandle();
		auto& registry = m_SceneRegistry->Registry;
		if (!registry.valid(enttHandle) || !registry.all_of<TransformComponent>(enttHandle))
			return nullptr;

		// Re-attach flow should not leak previous world body.
		if (registry.all_of<RigidBody3DComponent>(enttHandle))
			DestroyAttachedRigidBody(enttHandle);

		PhysicsRigidBodyDesc resolvedDesc = bodyDesc;
		const Transform& transform = registry.get<TransformComponent>(enttHandle).ObjectTransform;
		resolvedDesc.Transform.Position = transform.Position;
		resolvedDesc.Transform.Rotation = glm::quat(glm::radians(transform.Rotation));

		IPhysicsBody* body = CreateRigidBody(resolvedDesc, shape);
		if (!body)
			return nullptr;

		registry.emplace<RigidBody3DComponent>(enttHandle, RigidBody3DComponent{ body, syncMode });

		const bool shouldReadFromPhysics = syncMode == RigidBodySyncMode::ReadFromPhysics
			|| syncMode == RigidBodySyncMode::ReadWrite
			|| (syncMode == RigidBodySyncMode::Auto && body->GetType() != PhysicsBodyType::Kinematic);
		if (shouldReadFromPhysics)
		{
			const PhysicsTransform physicsTransform = body->GetTransform();
			auto& transformComponent = registry.get<TransformComponent>(enttHandle);
			transformComponent.ObjectTransform.Position = physicsTransform.Position;
			transformComponent.ObjectTransform.Rotation = glm::degrees(glm::eulerAngles(physicsTransform.Rotation));
		}

		return body;
	}

	IPhysicsBody* Scene::AttachRigidBody(const UUID& uuid,
	                                     const PhysicsRigidBodyDesc& bodyDesc,
	                                     const IPhysicsShape* shape,
	                                     RigidBodySyncMode syncMode)
	{
		return AttachRigidBody(TryGetEntityHandleByUUID(uuid), bodyDesc, shape, syncMode);
	}

	void Scene::DetachRigidBody(EntityHandle entityHandle)
	{
		Entity* entity = TryGetEntity(entityHandle);
		if (!entity)
			return;
		DestroyAttachedRigidBody(entity->GetEnttHandle());
	}

	void Scene::DetachRigidBody(const UUID& uuid)
	{
		DetachRigidBody(TryGetEntityHandleByUUID(uuid));
	}

	void Scene::OnEntityDestroyed(EntityHandle entityHandle)
	{
		auto entity = m_SceneRegistry->EntityPool.Get(entityHandle);
		if (!entity || !m_SceneRegistry->Registry.valid(entity->GetEnttHandle()))
			return;

		const entt::entity handle = entity->GetEnttHandle();
		auto& Registry = m_SceneRegistry->Registry;
		DestroyAttachedRigidBody(handle);
		if (Registry.all_of<IDComponent>(handle)) {
			const UUID& uuid = Registry.get<IDComponent>(handle).Value;
			m_SceneRegistry->EntityHandlersStore.erase(uuid);
		}
		m_SceneRegistry->EntityPool.Remove(entityHandle);
		m_SceneRegistry->Registry.destroy(handle);
	}

	void Scene::DestroyEntity(EntityHandle entity)
	{
		OnEntityDestroyed(entity);
	}

	EntityHandle Scene::TryGetEntityHandleByUUID(const UUID& uuid) const
	{
		auto it = m_SceneRegistry->EntityHandlersStore.find(uuid);
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

	UUID Scene::GetUUID(EntityHandle entityHandle) const
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

	void Scene::RemoveObject(const UUID& uuid)
	{
		DestroyEntity(TryGetEntityHandleByUUID(uuid));
	}

	void Scene::Clear()
	{
		auto rigidBodyView = m_SceneRegistry->Registry.view<RigidBody3DComponent>();
		for (const auto entity : rigidBodyView)
		{
			auto& rb = rigidBodyView.get<RigidBody3DComponent>(entity);
			if (rb.Body)
			{
				DestroyRigidBody(rb.Body);
				rb.Body = nullptr;
			}
		}

		m_SceneRegistry->EntityHandlersStore.clear();
		m_SceneRegistry->EntityPool.Clear();
		m_SceneRegistry->Registry.clear();
	}

	void Scene::SetPhysicsWorldDesc(const PhysicsWorldDesc& worldDesc)
	{
		m_PhysicsWorldDesc = worldDesc;
		
	}

	bool Scene::InitPhysicsWorld()
	{
		if (m_PhysicsWorld)
			return true;

		IPhysicsWorld* world = PhysicsFacade::CreateWorld(m_PhysicsWorldDesc);
		if (!world)
			return false;

		m_PhysicsWorld.reset(world);
		return true;
	}

	IPhysicsBody* Scene::CreateRigidBody(const PhysicsRigidBodyDesc& bodyDesc, const IPhysicsShape* shape)
	{
		if (!m_PhysicsWorld || !shape)
			return nullptr;
		return m_PhysicsWorld->CreateRigidBody(bodyDesc, shape);
	}

	void Scene::DestroyRigidBody(IPhysicsBody* body)
	{
		if (m_PhysicsWorld && body)
			m_PhysicsWorld->DestroyRigidBody(body);
	}

	bool Scene::Raycast(const glm::vec3& origin, const glm::vec3& direction,
	                    float maxDistance, PhysicsRaycastHit& outHit)
	{
		if (!m_PhysicsWorld)
		{
			outHit = {};
			return false;
		}
		return m_PhysicsWorld->Raycast(origin, direction, maxDistance, outHit);
	}

	bool Scene::OverlapSphere(const glm::vec3& center, float radius, PhysicsOverlapResult& outResult)
	{
		if (!m_PhysicsWorld)
		{
			outResult = {};
			return false;
		}
		return m_PhysicsWorld->OverlapSphere(center, radius, outResult);
	}

	bool Scene::OverlapBox(const glm::vec3& center, const glm::vec3& halfExtents,
	                       const glm::quat& rotation, PhysicsOverlapResult& outResult)
	{
		if (!m_PhysicsWorld)
		{
			outResult = {};
			return false;
		}
		return m_PhysicsWorld->OverlapBox(center, halfExtents, rotation, outResult);
	}

	bool Scene::OverlapCapsule(const glm::vec3& center, float radius, float halfHeight,
	                           const glm::quat& rotation, PhysicsOverlapResult& outResult)
	{
		if (!m_PhysicsWorld)
		{
			outResult = {};
			return false;
		}
		return m_PhysicsWorld->OverlapCapsule(center, radius, halfHeight, rotation, outResult);
	}

	void Scene::SetGravity(const glm::vec3& gravity)
	{
		if (m_PhysicsWorld)
			m_PhysicsWorld->SetGravity(gravity);
	}

	void Scene::SetContactListener(IPhysicsContactListener* listener)
	{
		if (m_PhysicsWorld)
			m_PhysicsWorld->SetContactListener(listener);
	}

	void Scene::SyncTransformsToPhysics()
	{
		auto view = m_SceneRegistry->Registry.view<TransformComponent, RigidBody3DComponent>();
		for (const auto entity : view)
		{
			if (!IsEntityBoundToHandle(entity))
				continue;

			auto& transformComponent = view.get<TransformComponent>(entity);
			auto& rigidBodyComponent = view.get<RigidBody3DComponent>(entity);
			IPhysicsBody* body = rigidBodyComponent.Body;
			if (!body)
				continue;

			const PhysicsBodyType bodyType = body->GetType();
			const bool shouldWrite = rigidBodyComponent.SyncMode == RigidBodySyncMode::WriteToPhysics
				|| rigidBodyComponent.SyncMode == RigidBodySyncMode::ReadWrite
				|| (rigidBodyComponent.SyncMode == RigidBodySyncMode::Auto && bodyType == PhysicsBodyType::Kinematic);
			if (!shouldWrite)
				continue;

			const Transform& transform = transformComponent.ObjectTransform;
			PhysicsTransform physicsTransform{};
			physicsTransform.Position = transform.Position;
			physicsTransform.Rotation = glm::quat(glm::radians(transform.Rotation));

			if (bodyType == PhysicsBodyType::Kinematic)
				body->SetKinematicTarget(physicsTransform);
			else
				body->SetTransform(physicsTransform);
		}
	}

	void Scene::SyncTransformsFromPhysics()
	{
		auto view = m_SceneRegistry->Registry.view<TransformComponent, RigidBody3DComponent>();
		for (const auto entity : view)
		{
			if (!IsEntityBoundToHandle(entity))
				continue;

			auto& transformComponent = view.get<TransformComponent>(entity);
			auto& rigidBodyComponent = view.get<RigidBody3DComponent>(entity);
			IPhysicsBody* body = rigidBodyComponent.Body;
			if (!body)
				continue;

			const PhysicsBodyType bodyType = body->GetType();
			const bool shouldRead = rigidBodyComponent.SyncMode == RigidBodySyncMode::ReadFromPhysics
				|| rigidBodyComponent.SyncMode == RigidBodySyncMode::ReadWrite
				|| (rigidBodyComponent.SyncMode == RigidBodySyncMode::Auto && bodyType != PhysicsBodyType::Kinematic);
			if (!shouldRead)
				continue;

			const PhysicsTransform physicsTransform = body->GetTransform();
			transformComponent.ObjectTransform.Position = physicsTransform.Position;
			transformComponent.ObjectTransform.Rotation = glm::degrees(glm::eulerAngles(physicsTransform.Rotation));
		}
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

	bool Scene::IsEnttEntityValid(entt::entity entity) const
	{
		return entity != entt::null && m_SceneRegistry->Registry.valid(entity);
	}

	bool Scene::IsEntityBoundToHandle(entt::entity entity) const
	{
		if (!IsEnttEntityValid(entity))
			return false;

		const auto& registry = m_SceneRegistry->Registry;
		if (!registry.all_of<IDComponent>(entity))
			return false;

		const UUID& uuid = registry.get<IDComponent>(entity).Value;
		const auto it = m_SceneRegistry->EntityHandlersStore.find(uuid);
		if (it == m_SceneRegistry->EntityHandlersStore.end())
			return false;

		Entity* pooledEntity = m_SceneRegistry->EntityPool.Get(it->second);
		return pooledEntity && pooledEntity->GetEnttHandle() == entity;
	}

	void Scene::DestroyAttachedRigidBody(entt::entity entity)
	{
		auto& registry = m_SceneRegistry->Registry;
		if (!registry.valid(entity) || !registry.all_of<RigidBody3DComponent>(entity))
			return;

		auto& rigidBody = registry.get<RigidBody3DComponent>(entity);
		if (rigidBody.Body)
		{
			DestroyRigidBody(rigidBody.Body);
			rigidBody.Body = nullptr;
		}

		registry.remove<RigidBody3DComponent>(entity);
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
	if constexpr (std::is_same_v<T, RigidBody3DComponent>)
	{
		auto& rb = m_SceneRegistry->Registry.get<RigidBody3DComponent>(enttHandler);
		if (rb.Body)
		{
			DestroyRigidBody(rb.Body);
			rb.Body = nullptr;
		}
	}
		m_SceneRegistry->Registry.remove<T>(enttHandler);
	}

	template TagComponent& Scene::AddComponent<TagComponent>(entt::entity, TagComponent&&);
	template TransformComponent& Scene::AddComponent<TransformComponent>(entt::entity);
	template MeshComponent& Scene::AddComponent<MeshComponent>(entt::entity);
	template ColorComponent& Scene::AddComponent<ColorComponent>(entt::entity);
	template VisibilityComponent& Scene::AddComponent<VisibilityComponent>(entt::entity);
	template LightComponent& Scene::AddComponent<LightComponent>(entt::entity, LightComponent&&);
	template RigidBody3DComponent& Scene::AddComponent<RigidBody3DComponent>(entt::entity, RigidBody3DComponent&&);

	template TagComponent& Scene::GetComponent<TagComponent>(entt::entity);
	template TransformComponent& Scene::GetComponent<TransformComponent>(entt::entity);
	template MeshComponent& Scene::GetComponent<MeshComponent>(entt::entity);
	template ColorComponent& Scene::GetComponent<ColorComponent>(entt::entity);
	template VisibilityComponent& Scene::GetComponent<VisibilityComponent>(entt::entity);
	template LightComponent& Scene::GetComponent<LightComponent>(entt::entity);
	template RigidBody3DComponent& Scene::GetComponent<RigidBody3DComponent>(entt::entity);
	template const TagComponent& Scene::GetComponent<TagComponent>(entt::entity) const;
	template const TransformComponent& Scene::GetComponent<TransformComponent>(entt::entity) const;
	template const MeshComponent& Scene::GetComponent<MeshComponent>(entt::entity) const;
	template const ColorComponent& Scene::GetComponent<ColorComponent>(entt::entity) const;
	template const VisibilityComponent& Scene::GetComponent<VisibilityComponent>(entt::entity) const;
	template const LightComponent& Scene::GetComponent<LightComponent>(entt::entity) const;
	template const RigidBody3DComponent& Scene::GetComponent<RigidBody3DComponent>(entt::entity) const;

	template bool Scene::HasComponent<TagComponent>(entt::entity) const;
	template bool Scene::HasComponent<TransformComponent>(entt::entity) const;
	template bool Scene::HasComponent<MeshComponent>(entt::entity) const;
	template bool Scene::HasComponent<ColorComponent>(entt::entity) const;
	template bool Scene::HasComponent<VisibilityComponent>(entt::entity) const;
	template bool Scene::HasComponent<LightComponent>(entt::entity) const;
	template bool Scene::HasComponent<RigidBody3DComponent>(entt::entity) const;

	template void Scene::RemoveComponent<TagComponent>(entt::entity);
	template void Scene::RemoveComponent<TransformComponent>(entt::entity);
	template void Scene::RemoveComponent<MeshComponent>(entt::entity);
	template void Scene::RemoveComponent<ColorComponent>(entt::entity);
	template void Scene::RemoveComponent<VisibilityComponent>(entt::entity);
	template void Scene::RemoveComponent<LightComponent>(entt::entity);
	template void Scene::RemoveComponent<RigidBody3DComponent>(entt::entity);

	template CHENGINE_API TagComponent* Scene::TryGetComponent<TagComponent>(EntityHandle);
	template CHENGINE_API IDComponent* Scene::TryGetComponent<IDComponent>(EntityHandle);
	template CHENGINE_API TransformComponent* Scene::TryGetComponent<TransformComponent>(EntityHandle);
	template CHENGINE_API MeshComponent* Scene::TryGetComponent<MeshComponent>(EntityHandle);
	template CHENGINE_API ColorComponent* Scene::TryGetComponent<ColorComponent>(EntityHandle);
	template CHENGINE_API VisibilityComponent* Scene::TryGetComponent<VisibilityComponent>(EntityHandle);
	template CHENGINE_API LightComponent* Scene::TryGetComponent<LightComponent>(EntityHandle);
	template CHENGINE_API RigidBody3DComponent* Scene::TryGetComponent<RigidBody3DComponent>(EntityHandle);

	template CHENGINE_API const TagComponent* Scene::TryGetComponent<TagComponent>(EntityHandle) const;
	template CHENGINE_API const IDComponent* Scene::TryGetComponent<IDComponent>(EntityHandle) const;
	template CHENGINE_API const TransformComponent* Scene::TryGetComponent<TransformComponent>(EntityHandle) const;
	template CHENGINE_API const MeshComponent* Scene::TryGetComponent<MeshComponent>(EntityHandle) const;
	template CHENGINE_API const ColorComponent* Scene::TryGetComponent<ColorComponent>(EntityHandle) const;
	template CHENGINE_API const VisibilityComponent* Scene::TryGetComponent<VisibilityComponent>(EntityHandle) const;
	template CHENGINE_API const LightComponent* Scene::TryGetComponent<LightComponent>(EntityHandle) const;
	template CHENGINE_API const RigidBody3DComponent* Scene::TryGetComponent<RigidBody3DComponent>(EntityHandle) const;

} // namespace CHEngine
