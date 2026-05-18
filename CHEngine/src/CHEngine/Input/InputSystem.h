#pragma once

#include "InputContext.h"
#include "ActionMap.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace CHEngine {

// Centralised input → action mapping.  Owns all state as member fields (RAII).
// One instance is created by the application and registered via RegisterInputSystem().
// Use GetInputSystem() to access it from anywhere.
class CHENGINE_API InputSystem {
public:
    InputSystem();
    ~InputSystem() = default;

    InputSystem(const InputSystem&)            = delete;
    InputSystem& operator=(const InputSystem&) = delete;

    bool LoadFromJson(const std::filesystem::path& path);
    void BeginFrame();

    void PushContext(std::string_view ctx);
    void PopContext(std::string_view ctx);
    bool IsActiveContext(std::string_view ctx) const;

    bool  Triggered(std::string_view action) const;
    bool  Down     (std::string_view action) const;
    bool  Released (std::string_view action) const;

    bool  IsModifierDown(uint8_t mods) const;

    enum class Axis { MouseDeltaX, MouseDeltaY, MouseWheel };
    float GetAxis(Axis axis) const;

private:
    ActionMap                  m_Map;
    std::vector<InputContext>  m_ContextStack;

    std::unordered_map<std::string, bool> m_Triggered;
    std::unordered_map<std::string, bool> m_Down;
    std::unordered_map<std::string, bool> m_Released;

    float   m_MouseDX     = 0.0f;
    float   m_MouseDY     = 0.0f;
    float   m_MouseWheel  = 0.0f;
    uint8_t m_CurrentMods = 0;
};

// ── Module-level accessor ─────────────────────────────────────────────────────
// Application calls RegisterInputSystem() at startup and passes nullptr on shutdown.
CHENGINE_API void RegisterInputSystem(InputSystem* sys);
CHENGINE_API InputSystem& GetInputSystem();

} // namespace CHEngine
