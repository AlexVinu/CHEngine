#include "chepch.h"
#include "Scene.h"
#include "Components.h"

#include <entt/entt.hpp>

namespace CHEngine {

// ---------------------------------------------------------------------------
// Pimpl struct — entt lives only in this translation unit
// ---------------------------------------------------------------------------
struct Scene::SceneRegistry {
	entt::registry registry;
};

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

Scene::Scene()
	: m_pRegistry(std::make_unique<SceneRegistry>())
{}

Scene::~Scene() = default;

Scene::Scene(Scene&&) = default;
Scene& Scene::operator=(Scene&&) = default;

// ---------------------------------------------------------------------------
// Helper (local to this TU): mirror SceneObject into ECS
// Returns the entt handle so AddModel can patch MeshComponent.SourcePath
// ---------------------------------------------------------------------------
static entt::entity CreateECSEntity(entt::registry& reg,
                                    const std::string& name, uint32_t id)
{
	entt::entity e = reg.create();
	reg.emplace<TagComponent>(e, TagComponent{name, id});
	reg.emplace<TransformComponent>(e);
	reg.emplace<MeshComponent>(e);
	reg.emplace<ColorComponent>(e);
	reg.emplace<VisibilityComponent>(e);
	return e;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

SceneObject* Scene::AddObject(const std::string& name)
{
	auto obj = std::make_unique<SceneObject>(name);
	SceneObject* ptr = obj.get();
	m_IDIndex[ptr->ID] = ptr;
	m_Objects.push_back(std::move(obj));

	CreateECSEntity(m_pRegistry->registry, ptr->Name, ptr->ID);

	return ptr;
}

SceneObject* Scene::AddLight(const std::string& name, LightType type)
{
	auto obj = std::make_unique<SceneObject>(name);
	obj->LightData.Type = type;
	SceneObject* ptr = obj.get();
	m_IDIndex[ptr->ID] = ptr;
	m_Objects.push_back(std::move(obj));

	CreateECSEntity(m_pRegistry->registry, ptr->Name, ptr->ID);

	return ptr;
}

SceneObject* Scene::AddModel(const std::string& name, std::vector<Mesh>&& meshes,
                             const std::string& sourcePath)
{
	auto obj = std::make_unique<SceneObject>(name);
	obj->Meshes = std::move(meshes);
	SceneObject* ptr = obj.get();
	m_IDIndex[ptr->ID] = ptr;
	m_Objects.push_back(std::move(obj));

	entt::entity e = CreateECSEntity(m_pRegistry->registry, ptr->Name, ptr->ID);
	auto& mc = m_pRegistry->registry.get<MeshComponent>(e);
	mc.Meshes = ptr->Meshes;
	mc.SourcePath = sourcePath;

	return ptr;
}

void Scene::RemoveObject(uint32_t id)
{
	m_IDIndex.erase(id);
	m_Objects.erase(
		std::remove_if(m_Objects.begin(), m_Objects.end(),
			[id](const std::unique_ptr<SceneObject>& obj) { return obj->ID == id; }),
		m_Objects.end()
	);

	// Find entity by ID and destroy it
	auto view = m_pRegistry->registry.view<TagComponent>();
	for (auto e : view) {
		if (view.get<TagComponent>(e).ID == id) {
			m_pRegistry->registry.destroy(e);
			break;
		}
	}
}

SceneObject* Scene::FindByID(uint32_t id)
{
	auto it = m_IDIndex.find(id);
	return (it != m_IDIndex.end()) ? it->second : nullptr;
}

void Scene::Clear()
{
	m_Objects.clear();
	m_IDIndex.clear();
	m_pRegistry->registry.clear();
}

std::string Scene::GetMeshSourcePath(uint32_t objectID) const
{
	auto view = m_pRegistry->registry.view<TagComponent, MeshComponent>();
	for (auto e : view) {
		if (view.get<TagComponent>(e).ID == objectID)
			return view.get<MeshComponent>(e).SourcePath;
	}
	return {};
}

} // namespace CHEngine
