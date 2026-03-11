#pragma once

#include <Core.h>
#include "SceneObject.h"
#include <vector>
#include <memory>

namespace CHEngine {

	class CHENGINE_API Scene
	{
	public:
		Scene() = default;

		SceneObject* AddObject(const std::string& name = "Object");
		SceneObject* AddModel(const std::string& name, std::vector<Mesh>&& meshes);
		void RemoveObject(uint32_t id);
		SceneObject* FindByID(uint32_t id);

		std::vector<std::unique_ptr<SceneObject>>& GetObjects() { return m_Objects; }
		const std::vector<std::unique_ptr<SceneObject>>& GetObjects() const { return m_Objects; }

	private:
		std::vector<std::unique_ptr<SceneObject>> m_Objects;
	};

}
