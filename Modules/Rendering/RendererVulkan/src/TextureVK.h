#pragma once

#include "Render/Core/RenderTypes.h"
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <cstdint>

namespace CHModules {

    class TextureVK
    {
    public:
        TextureVK(const uint8_t* data,
                  uint32_t width,
                  uint32_t height,
                  uint32_t channels,
                  uint32_t mipLevels,
                  uint32_t arrayLayers,
                  CHEngine::TextureFormat format,
                  CHEngine::TextureType   type,
                  CHEngine::TextureUsage  usage,
                  CHEngine::MemoryType    memory,
                  const char* debugName);
        ~TextureVK();

        TextureVK(const TextureVK&)            = delete;
        TextureVK& operator=(const TextureVK&) = delete;

        VkImage     GetImage()    const { return m_Image; }
        VkImageView GetView()     const { return m_ImageView; }
        VkSampler   GetSampler()  const { return m_Sampler; }
        uint32_t    GetWidth()    const { return m_Width; }
        uint32_t    GetHeight()   const { return m_Height; }
        VkFormat    GetVkFormat() const { return m_VkFormat; }

        // Current layout — updated by FrameGraphBackendVK during barrier recording.
        VkImageLayout GetLayout() const          { return m_CurrentLayout; }
        void SetLayout(VkImageLayout l)          { m_CurrentLayout = l; }

        static VkFormat    ToVkFormat(CHEngine::TextureFormat fmt);

    private:
        static VkImageType ToVkImageType(CHEngine::TextureType type);

        VkImage       m_Image       = VK_NULL_HANDLE;
        VmaAllocation m_Alloc       = VK_NULL_HANDLE;
        VkImageView   m_ImageView   = VK_NULL_HANDLE;
        VkSampler     m_Sampler     = VK_NULL_HANDLE;
        VkFormat      m_VkFormat    = VK_FORMAT_R8G8B8A8_UNORM;
        VkImageLayout m_CurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        // Lazily created VkDescriptorSet for ImGui::Image() display.
        // Populated on first GetTextureNativeID() call.
        VkDescriptorSet m_ImGuiDescSet = VK_NULL_HANDLE;

    public:
        VkDescriptorSet& ImGuiDescSet() { return m_ImGuiDescSet; }

        uint32_t m_Width  = 0;
        uint32_t m_Height = 0;
    };

} // namespace CHModules
