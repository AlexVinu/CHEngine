#pragma once

#include <Core.h>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include "SceneObject.h"

namespace CHEngine {

class CHENGINE_API Scene
{
public:
	Scene();
	~Scene();  // Must be declared (not defaulted here) for unique_ptr<incomplete type>

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

	// Returns the source file path (mesh import path) stored in ECS for the given object ID.
	// Returns empty string if not found. Avoids exposing entt registry publicly.
	std::string GetMeshSourcePath(uint32_t objectID) const;

private:
	std::vector<std::unique_ptr<SceneObject>> m_Objects;
	std::unordered_map<uint32_t, SceneObject*> m_IDIndex; // O(1) lookup by ID

	// ECS registry — hidden from public header to avoid cross-dylib entt ABI issues
	struct SceneRegistry;
	std::unique_ptr<SceneRegistry> m_pRegistry;
};

} // namespace CHEngine
