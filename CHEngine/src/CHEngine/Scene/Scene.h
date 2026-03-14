#pragma once

#include <Core.h>
#include "SceneObject.h"
#include "Components.h"
#include "Entity.h"
#include <vector>
#include <memory>
#include <string>

#include <entt/entt.hpp>

namespace CHEngine {

	class CHENGINE_API Scene
	{
	public:
		Scene() = default;

		Scene(const Scene&) = delete;
		Scene& operator=(const Scene&) = delete;
		Scene(Scene&&) = default;
		Scene& operator=(Scene&&) = default;

		SceneObject* AddObject(const std::string& name = "Object");
		SceneObject* AddModel(const std::string& name, std::vector<Mesh>&& meshes,
		                      const std::string& sourcePath = "");
		void RemoveObject(uint32_t id);
		SceneObject* FindByID(uint32_t id);
		void Clear();

		std::vector<std::unique_ptr<SceneObject>>& GetObjects() { return m_Objects; }
		const std::vector<std::unique_ptr<SceneObject>>& GetObjects() const { return m_Objects; }

		// ECS access
		entt::registry&       GetRegistry()       { return m_Registry; }
		const entt::registry& GetRegistry() const { return m_Registry; }

		// Create entity with all standard components
		Entity CreateEntity(const std::string& name, uint32_t id);
		void   DestroyEntity(Entity entity);

	private:
		std::vector<std::unique_ptr<SceneObject>> m_Objects;
		entt::registry m_Registry;
	};

}
