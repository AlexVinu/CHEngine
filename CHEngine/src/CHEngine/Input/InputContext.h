#pragma once

#include <string>
#include <string_view>

namespace CHEngine {

// Context is a plain string — games define their own contexts freely.
// Common contexts are declared as constants below.
using InputContext = std::string;

namespace InputContexts {
    inline constexpr std::string_view Editor = "Editor";
    inline constexpr std::string_view Game   = "Game";
} // namespace InputContexts

} // namespace CHEngine
