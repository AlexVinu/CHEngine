#pragma once

#include <cstdint>

namespace CHModules {
namespace VKGlobals {

    /// VulkanContext* — владелец: RenderApiVK (Init / Shutdown).
    inline void* g_ContextPtr = nullptr;

} // namespace VKGlobals
} // namespace CHModules
