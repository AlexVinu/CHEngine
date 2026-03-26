#pragma once
#include <Core.h>
#include <entt/entt.hpp>

namespace CHEngine {

class Scene;

class CHENGINE_API Entity {
public:
    Entity() = default;
    Entity(entt::entity handle, Scene* scene) : m_EntityHandle(handle), m_Scene(scene) {}

    template<typename T, typename... Args>
    T& AddComponent(Args&&... args);

    template<typename T>
    T& GetComponent();

    template<typename T>
    bool HasComponent() const;

    template<typename T>
    void RemoveComponent();

    bool IsValid() const { return m_EntityHandle != entt::null && m_Scene != nullptr; }
    operator bool() const { return IsValid(); }
    operator entt::entity() const { return m_EntityHandle; }
    entt::entity GetHandle() const { return m_EntityHandle; }

    bool operator==(const Entity& other) const
    {
        return m_EntityHandle == other.m_EntityHandle && m_Scene == other.m_Scene;
    }
    bool operator!=(const Entity& other) const { return !(*this == other); }

private:
    entt::entity m_EntityHandle = entt::null;
    Scene* m_Scene = nullptr;
};

}
