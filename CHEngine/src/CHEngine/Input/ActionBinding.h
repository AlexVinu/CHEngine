#pragma once

#include <cstdint>

namespace CHEngine {

enum ModBits : uint8_t {
    Mod_None  = 0,
    Mod_Ctrl  = 1 << 0,  // unified with Super (Cmd on macOS)
    Mod_Shift = 1 << 1,
    Mod_Alt   = 1 << 2,
};

enum class TriggerType {
    Pressed,   // edge: this frame
    Down,      // held
    Released,  // edge: released this frame
    Drag,      // mouse only: IsMouseDragging
};

struct ActionBinding {
    int         key           = -1;   // CHEngine::Key code; -1 = not bound
    int         mouseButton   = -1;   // CHEngine::MouseButton code; -1 = not bound
    uint8_t     mods          = Mod_None;
    TriggerType trigger       = TriggerType::Pressed;
    float       dragThreshold = 0.0f;
};

} // namespace CHEngine
