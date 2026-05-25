#pragma once

#include "RendererVK/VulkanSharedContext.h"

namespace CHModules {
namespace VKGlobals {

    // VulkanContext* — owned by RenderFactoryVK (Init / Shutdown).
    inline void* g_ContextPtr = nullptr;

    // Module-local shared context, owned by RenderFactoryVK and exposed to
    // other Vulkan-aware modules (ImGuiVK) via IRenderFactory::GetNativeSharedContext().
    // Backed here so TextureVK::~TextureVK can call UnregisterImGuiTexture
    // without needing a handle back to the factory.
    inline VulkanSharedContext g_SharedContext;

} // namespace VKGlobals
} // namespace CHModules
