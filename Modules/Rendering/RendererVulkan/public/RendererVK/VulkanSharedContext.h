#pragma once

#include <cstdint>

// Shared context published by RenderFactoryVK and consumed by ImGuiVK
// (or any other Vulkan-aware module). Retrieved through
// IRenderFactory::GetNativeSharedContext().
//
// All Vulkan handles are stored as void* so this header does not require
// <vulkan/vulkan.h>; the consumer cast them back to Vk* types.

namespace CHModules {

    struct VulkanSharedContext
    {
        // Set once at RenderFactoryVK::Init
        void*    Instance       = nullptr;   // VkInstance
        void*    PhysicalDevice = nullptr;   // VkPhysicalDevice
        void*    Device         = nullptr;   // VkDevice
        void*    Queue          = nullptr;   // VkQueue
        uint32_t QueueFamily    = 0;
        uint32_t MinImageCount  = 2;
        uint32_t ImageCount     = 2;
        uint32_t SwapchainFormat = 0;        // VkFormat value

        // Updated each frame by RenderFactoryVK::BeginFrame()
        void*    CurrentCmdBuffer     = nullptr;  // VkCommandBuffer
        void*    CurrentSwapchainView = nullptr;  // VkImageView
        uint32_t SwapchainWidth       = 0;
        uint32_t SwapchainHeight      = 0;

        // Set by ImGuiVK after init. Wraps a VkImageView+VkSampler into a
        // VkDescriptorSet usable as ImTextureID. Returns the VkDescriptorSet
        // as uint64_t, or 0 on failure.
        uint64_t (*RegisterImGuiTexture)(void* sampler, void* imageView, uint32_t imageLayout) = nullptr;

        // Cleans up a descriptor set previously returned by RegisterImGuiTexture.
        // Must be called before destroying the underlying sampler/image view.
        void (*UnregisterImGuiTexture)(uint64_t descriptorSet) = nullptr;
    };

} // namespace CHModules
