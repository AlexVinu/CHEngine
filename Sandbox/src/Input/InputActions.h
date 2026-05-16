#pragma once

#include "InputContext.h"

#include <filesystem>
#include <string_view>

namespace Sandbox {

// Centralized input → action mapping. Bindings declared in
// `config/keybindings.json`. Use `Triggered`/`Down`/`Released` to query.
class InputActions {
public:
    static bool LoadFromJson(const std::filesystem::path& path);
    static void BeginFrame();

    static void PushContext(InputContext ctx);
    static void PopContext(InputContext ctx);
    static bool IsActiveContext(InputContext ctx);

    static bool Triggered(std::string_view action);
    static bool Down(std::string_view action);
    static bool Released(std::string_view action);

    enum class Axis { MouseDeltaX, MouseDeltaY, MouseWheel };
    static float GetAxis(Axis axis);
};

} // namespace Sandbox
