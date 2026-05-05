#include "TextureVK.h"

namespace CHModules {

    TextureVK::TextureVK(const uint8_t* /*data*/, uint32_t width, uint32_t height, uint32_t /*channels*/)
        : m_Width(width), m_Height(height)
    {
        // TODO: создать VkImage, VkImageView, VkSampler, загрузить пиксели
    }

    TextureVK::~TextureVK()
    {
        // TODO: vkDestroyImageView, vkDestroyImage, vkDestroySampler, vkFreeMemory
    }

}
