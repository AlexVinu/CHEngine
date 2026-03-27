#pragma once
#include <Core.h>

#include "Scene.h"

namespace CHEngine {

class CHENGINE_API Entity {
public:
    Entity() = default;
    Entity(entt::entity handle, Scene* scene) : m_EnttHandle(handle), m_Scene(scene) {}

	template<typename T, typename... Args>
	T& AddComponent(Args&&... args)
	{
		CHE_CORE_ASSERT(m_Scene, "Entity has no owning scene");

		CHE_CORE_ASSERT(IsValid(), "Entity is not tracked by owning scene");
		return m_Scene->AddComponent<T>(m_EnttHandle, std::forward<Args>(args)...);
	}

	template<typename T>
	T& GetComponent()
	{
		CHE_CORE_ASSERT(m_Scene, "Entity has no owning scene");

		CHE_CORE_ASSERT(IsValid(), "Entity is not tracked by owning scene");
		return m_Scene->GetComponent<T>(m_EnttHandle);
	}

	template<typename T>
	bool HasComponent() const
	{
		CHE_CORE_ASSERT(m_Scene, "Entity has no owning scene");

		return IsValid() && m_Scene->HasComponent<T>(m_EnttHandle);
	}

	template<typename T>
	void RemoveComponent()
	{
		CHE_CORE_ASSERT(m_Scene, "Entity has no owning scene");

		CHE_CORE_ASSERT(IsValid(), "Entity is not tracked by owning scene");
		m_Scene->RemoveComponent<T>(m_EnttHandle);
	}

    bool IsValid() const
    {
        return m_Scene != nullptr && m_Scene->IsEnttEntityValid(m_EnttHandle);
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
