#pragma once
#include <Core.h>
#include <cstdint>

// WARNING !!!!
// Это тут не должно быть, но так хотя бы работает
namespace CHEngine {

    // Populated by RendererVK after Init(); read by ImGuiVK.
    // All Vulkan handles stored as void* to keep CHEngine_CORE renderer-agnostic.
    struct VulkanSharedState
    {
        // Set once at Init
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

        // Set by ImGuiVK after init.
        // Wraps a VkImageView+VkSampler into a VkDescriptorSet usable as ImTextureID.
        // sampler/imageView are void*, layout is VkImageLayout (uint32_t).
        // Returns the VkDescriptorSet as uint64_t, or 0 on failure.
        uint64_t (*RegisterImGuiTexture)(void* sampler, void* imageView, uint32_t imageLayout) = nullptr;

        // Cleans up a descriptor set previously returned by RegisterImGuiTexture.
        // Must be called before destroying the underlying sampler/image view.
        // descriptorSet is the uint64_t returned by RegisterImGuiTexture.
        void (*UnregisterImGuiTexture)(uint64_t descriptorSet) = nullptr;
    };

    CHE_CORE_API VulkanSharedState& GetVulkanSharedState();

} // namespace CHEngine
