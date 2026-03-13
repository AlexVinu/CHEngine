#include "chepch.h"
#include "Scene.h"

namespace CHEngine {

	SceneObject* Scene::AddObject(const std::string& name)
	{
		auto obj = std::make_unique<SceneObject>(name);
		SceneObject* ptr = obj.get();
		m_Objects.push_back(std::move(obj));
		return ptr;
	}

	SceneObject* Scene::AddModel(const std::string& name, std::vector<Mesh>&& meshes)
	{
		auto obj = std::make_unique<SceneObject>(name);
		obj->Meshes = std::move(meshes);
		SceneObject* ptr = obj.get();
		m_Objects.push_back(std::move(obj));
		return ptr;
	}

	void Scene::RemoveObject(uint32_t id)
	{
		m_Objects.erase(
			std::remove_if(m_Objects.begin(), m_Objects.end(),
				[id](const std::unique_ptr<SceneObject>& obj) { return obj->ID == id; }),
			m_Objects.end()
		);
	}

	SceneObject* Scene::FindByID(uint32_t id)
	{
		for (auto& obj : m_Objects)
			if (obj->ID == id)
				return obj.get();
		return nullptr;
	}

}
