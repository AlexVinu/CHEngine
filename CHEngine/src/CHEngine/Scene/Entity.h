#pragma once
#include <Core.h>

#include "Scene.h"

namespace CHEngine {

	// Wrapper for handle and scene ptr
	// Not a super special thing

class CHENGINE_API Entity {
public:
    Entity() = default;
    Entity(entt::entity handle, Scene* scene) : m_EnttHandle(handle), m_Scene(scene) {}

	template<typename T, typename... Args>
	T& AddComponent(Args&&... args)
	{
		CHE_CORE_ASSERT(m_Scene, "Entity has no owning scene");
		CHE_CORE_ASSERT(IsValid(), "Entity is not tracked by owning scene");
		return m_Scene->m_SceneRegistry->Registry.emplace<T>(m_EnttHandle, std::forward<Args>(args)...);
	}

	template<typename T>
	T& GetComponent()
	{
		CHE_CORE_ASSERT(m_Scene, "Entity has no owning scene");
		CHE_CORE_ASSERT(IsValid(), "Entity is not tracked by owning scene");
		return m_Scene->m_SceneRegistry->Registry.get<T>(m_EnttHandle);
	}

	template<typename T>
	const T& GetComponent() const
	{
		CHE_CORE_ASSERT(m_Scene, "Entity has no owning scene");
		CHE_CORE_ASSERT(IsValid(), "Entity is not tracked by owning scene");
		return m_Scene->m_SceneRegistry->Registry.get<T>(m_EnttHandle);
	}

	template<typename T>
	bool HasComponent() const
	{
		CHE_CORE_ASSERT(m_Scene, "Entity has no owning scene");
		CHE_CORE_ASSERT(IsValid(), "Entity is not tracked by owning scene");
		return m_Scene->m_SceneRegistry->Registry.all_of<T>(m_EnttHandle);
	}

	template<typename T>
	void RemoveComponent()
	{
		CHE_CORE_ASSERT(m_Scene, "Entity has no owning scene");
		CHE_CORE_ASSERT(IsValid(), "Entity is not tracked by owning scene");
		m_Scene->m_SceneRegistry->Registry.remove<T>(m_EnttHandle);
	}

    bool IsValid() const
    {
        return m_Scene != nullptr && m_EnttHandle != entt::null;
    }
    operator bool() const { return IsValid(); }
    operator entt::entity() const { return m_EnttHandle; }
    entt::entity GetEnttHandle() const { return m_EnttHandle; }

    bool operator==(const Entity& other) const
    {
        return m_EnttHandle == other.m_EnttHandle && m_Scene == other.m_Scene;
    }
    bool operator!=(const Entity& other) const { return !(*this == other); }

private:
    entt::entity m_EnttHandle = entt::null;
    Scene* m_Scene = nullptr;
};

}
