#include "chepch.h"
#include "Scene.h"
#include "Entity.h"
#include "Components.h"

#include <unordered_set>
#include <entt/entt.hpp>
namespace CHEngine {

	static const std::string kEmptyString{};

	// ── String pool ──────────────────────────────────────────────────────────

	StringID Scene::InternString(const std::string& str)
	{
		if (str.empty()) return INVALID_ID<StringID>;
		auto it = m_StringLookup.find(str);
		if (it != m_StringLookup.end()) return it->second;
		StringID id = m_NextStringID++;
		StringPool[id] = str;
		m_StringLookup[str] = id;
		return id;
	}

	const std::string& Scene::GetString(StringID id) const
	{
		if (id == INVALID_ID<StringID>) return kEmptyString;
		auto it = StringPool.find(id);
		return it != StringPool.end() ? it->second : kEmptyString;
	}

	// ── Script pool ──────────────────────────────────────────────────────────

	ScriptsID Scene::AllocateScripts()
	{
		ScriptsID id = m_NextScriptsID++;
		ScriptPool[id] = {};
		return id;
	}

	const std::vector<ScriptEntry>* Scene::GetScripts(ScriptsID id) const
	{
		if (id == INVALID_ID<ScriptsID>) return nullptr;
		auto it = ScriptPool.find(id);
		return it != ScriptPool.end() ? &it->second : nullptr;
	}

	std::vector<ScriptEntry>* Scene::GetScripts(ScriptsID id)
	{
		if (id == INVALID_ID<ScriptsID>) return nullptr;
		auto it = ScriptPool.find(id);
		return it != ScriptPool.end() ? &it->second : nullptr;
	}

	void Scene::LoadStringPoolEntry(StringID id, const std::string& str)
	{
		if (id == INVALID_ID<StringID> || str.empty()) return;
		StringPool[id] = str;
		m_StringLookup[str] = id;
		if (id >= m_NextStringID) m_NextStringID = id + 1;
	}

	void Scene::LoadScriptPoolEntry(ScriptsID id, std::vector<ScriptEntry> scripts)
	{
		if (id == INVALID_ID<ScriptsID>) return;
		ScriptPool[id] = std::move(scripts);
		if (id >= m_NextScriptsID) m_NextScriptsID = id + 1;
	}

	void Scene::CollectGarbage()
	{
		auto& reg = m_SceneRegistry->Registry;

		std::unordered_set<StringID>  liveStrings;
		std::unordered_set<ScriptsID> liveScripts;

		auto markString = [&](StringID id) {
			if (id != INVALID_ID<StringID>) liveStrings.insert(id);
		};

		// Gather every StringID still referenced by a live component.
		reg.view<TagComponent>().each([&](TagComponent& c)      { markString(c.Name); });
		reg.view<MeshComponent>().each([&](MeshComponent& c)    { markString(c.SourcePath); });
		reg.view<UIImageComponent>().each([&](UIImageComponent& c) { markString(c.TexturePath); });
		reg.view<UITextComponent>().each([&](UITextComponent& c) {
			markString(c.Text);
			markString(c.FontPath);
		});

		// Gather every ScriptsID still referenced by a live component.
		reg.view<ScriptComponent>().each([&](ScriptComponent& c) {
			if (c.Scripts != INVALID_ID<ScriptsID>) liveScripts.insert(c.Scripts);
		});

		// Sweep StringPool, keeping m_StringLookup in sync (it is keyed by the string).
		for (auto it = StringPool.begin(); it != StringPool.end(); )
		{
			if (liveStrings.find(it->first) == liveStrings.end())
			{
				m_StringLookup.erase(it->second);
				it = StringPool.erase(it);
			}
			else
			{
				++it;
			}
		}

		// Sweep ScriptPool.
		for (auto it = ScriptPool.begin(); it != ScriptPool.end(); )
		{
			if (liveScripts.find(it->first) == liveScripts.end())
				it = ScriptPool.erase(it);
			else
				++it;
		}
	}

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

		StringPool      = std::move(other.StringPool);
		ScriptPool      = std::move(other.ScriptPool);
		m_StringLookup  = std::move(other.m_StringLookup);
		m_NextStringID  = other.m_NextStringID;
		m_NextScriptsID = other.m_NextScriptsID;
		WorldScripts    = std::move(other.WorldScripts);

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

		// 1. Copy pools first so InternString calls in CreateEntity find existing IDs.
		StringPool      = source.StringPool;
		ScriptPool      = source.ScriptPool;
		m_StringLookup  = source.m_StringLookup;
		m_NextStringID  = source.m_NextStringID;
		m_NextScriptsID = source.m_NextScriptsID;

		auto& srcReg = source.m_SceneRegistry->Registry;
		auto& dstReg = m_SceneRegistry->Registry;

		// 2. Create all entities preserving UUIDs and names.
		auto srcView = srcReg.view<IDComponent, TagComponent>();
		for (entt::entity srcEntt : srcView)
		{
			const auto& id  = srcReg.get<IDComponent>(srcEntt);
			const auto& tag = srcReg.get<TagComponent>(srcEntt);
			// GetString returns the interned name; CreateEntity re-interns → same ID.
			CreateEntity(source.GetString(tag.Name), id.Value);
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

		entity->AddComponent<TagComponent>(TagComponent{ InternString(name.empty() ? "Object" : name) });
		entity->AddComponent<IDComponent>(IDComponent{ uuid });
		entity->AddComponent<VisibilityComponent>();

		const EntityHandle handle = m_SceneRegistry->EntityPool.Add(entity);
		m_SceneRegistry->EntityHandlersStore[uuid] = handle;
		m_SceneRegistry->EnttToHandleStore[enttEntity] = handle;

		return handle;
	}

	EntityHandle Scene::CreateEntity(const std::string& name)
	{
		return CreateEntity(name, UUID::Generate());
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
			return UUID::Nil();

		const entt::entity handle = entity->GetEnttHandle();
		const auto& Registry = m_SceneRegistry->Registry;
		if (!Registry.all_of<IDComponent>(handle))
			return UUID::Nil();
		return Registry.get<IDComponent>(handle).Value;
	}

	const CHEngine::UUID Scene::GetUUIDByString(const std::string_view name) const
	{
		const auto it = m_StringLookup.find(name);
		if (it == m_StringLookup.end())
			return UUID::Nil();

		const StringID stringID = it->second;
		for (const auto& [uuid, handle] : m_SceneRegistry->EntityHandlersStore)
		{
			const Entity* entity = m_SceneRegistry->EntityPool.Get(handle);
			if (!entity || !entity->IsValid())
				continue;

			const entt::entity enttEntity = entity->GetEnttHandle();
			if (!m_SceneRegistry->Registry.all_of<TagComponent>(enttEntity))
				continue;

			const TagComponent& tag = m_SceneRegistry->Registry.get<TagComponent>(enttEntity);
			if (tag.Name == stringID)
				return uuid;
		}

		return UUID::Nil();
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
		StringPool.clear();
		ScriptPool.clear();
		m_StringLookup.clear();
		m_NextStringID  = 0;
		m_NextScriptsID = 0;
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
			case PhysShapeType::Box:
				scaledShapeDesc.HalfExtents = absScale;
				break;
			case PhysShapeType::Sphere:
				scaledShapeDesc.Radius = maxAxisAbsScale;
				break;
			case PhysShapeType::Capsule:
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
