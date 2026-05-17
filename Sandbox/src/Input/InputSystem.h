#pragma once

#include "InputContext.h"
#include "ActionMap.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Sandbox {

// Centralised input → action mapping.  Owns all state as member fields (RAII).
// One instance is created by SandboxApp and registered via RegisterInputSystem().
// Use GetInputSystem() to access it from anywhere in Sandbox.
class InputSystem {
public:
    InputSystem();
    ~InputSystem() = default;

    InputSystem(const InputSystem&) = delete;
    InputSystem& operator=(const InputSystem&) = delete;

    bool LoadFromJson(const std::filesystem::path& path);
    void BeginFrame();

    void PushContext(InputContext ctx);
    void PopContext(InputContext ctx);
    bool IsActiveContext(InputContext ctx) const;

    bool  Triggered(std::string_view action) const;
    bool  Down     (std::string_view action) const;
    bool  Released (std::string_view action) const;

    enum class Axis { MouseDeltaX, MouseDeltaY, MouseWheel };
    float GetAxis(Axis axis) const;

private:
    ActionMap                             m_Map;
    std::vector<InputContext>             m_ContextStack;

    std::unordered_map<std::string, bool> m_Triggered;
    std::unordered_map<std::string, bool> m_Down;
    std::unordered_map<std::string, bool> m_Released;

    float m_MouseDX    = 0.0f;
    float m_MouseDY    = 0.0f;
    float m_MouseWheel = 0.0f;
};

// ── Module-level accessor ─────────────────────────────────────────────────────
// SandboxApp calls RegisterInputSystem() at startup and resets it on shutdown.
void          RegisterInputSystem(InputSystem* sys);
InputSystem&  GetInputSystem();

} // namespace Sandbox
