#include "chepch.h"
#include "Scene.h"

namespace CHEngine {

	SceneObject* Scene::AddObject(const std::string& name)
	{
		auto obj = std::make_unique<SceneObject>(name);
		SceneObject* ptr = obj.get();
		m_Objects.push_back(std::move(obj));

		// Mirror into ECS
		CreateEntity(ptr->Name, ptr->ID);

		return ptr;
	}

	SceneObject* Scene::AddModel(const std::string& name, std::vector<Mesh>&& meshes,
	                             const std::string& sourcePath)
	{
		auto obj = std::make_unique<SceneObject>(name);
		obj->Meshes = std::move(meshes);
		SceneObject* ptr = obj.get();
		m_Objects.push_back(std::move(obj));

		// Mirror into ECS - copy meshes into MeshComponent
		Entity e = CreateEntity(ptr->Name, ptr->ID);
		auto& mc = m_Registry.get<MeshComponent>(static_cast<entt::entity>(e.GetRawHandle()));
		mc.Meshes = ptr->Meshes;
		mc.SourcePath = sourcePath;

		return ptr;
	}

	void Scene::RemoveObject(uint32_t id)
	{
		m_Objects.erase(
			std::remove_if(m_Objects.begin(), m_Objects.end(),
				[id](const std::unique_ptr<SceneObject>& obj) { return obj->ID == id; }),
			m_Objects.end()
		);

		// Find entity by ID and destroy it
		auto view = m_Registry.view<TagComponent>();
		for (auto e : view) {
			if (view.get<TagComponent>(e).ID == id) {
				m_Registry.destroy(e);
				break;
			}
		}
	}

	SceneObject* Scene::FindByID(uint32_t id)
	{
		for (auto& obj : m_Objects)
			if (obj->ID == id)
				return obj.get();
		return nullptr;
	}

	void Scene::Clear()
	{
		m_Objects.clear();
		m_Registry.clear();
	}

	Entity Scene::CreateEntity(const std::string& name, uint32_t id)
	{
		entt::entity e = m_Registry.create();
		m_Registry.emplace<TagComponent>(e, TagComponent{name, id});
		m_Registry.emplace<TransformComponent>(e);
		m_Registry.emplace<MeshComponent>(e);
		m_Registry.emplace<ColorComponent>(e);
		m_Registry.emplace<VisibilityComponent>(e);
		return Entity(static_cast<uint32_t>(e), this);
	}

	void Scene::DestroyEntity(Entity entity)
	{
		m_Registry.destroy(static_cast<entt::entity>(entity.GetRawHandle()));
	}

}
