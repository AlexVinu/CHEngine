#pragma once
#include <cstdint>
#include <Core.h>

namespace CHEngine {

class Scene;

class CHENGINE_API Entity {
public:
    Entity() = default;
    Entity(uint32_t handle, Scene* scene) : m_Handle(handle), m_Scene(scene) {}

    bool     IsValid() const { return m_Handle != 0xFFFFFFFF && m_Scene != nullptr; }
    uint32_t GetRawHandle() const { return m_Handle; }

    // Component methods are NOT templated here to avoid cross-dylib entt issues.
    // Use Scene's typed methods instead.

private:
    uint32_t m_Handle = 0xFFFFFFFF;
    Scene*   m_Scene  = nullptr;
};

}
