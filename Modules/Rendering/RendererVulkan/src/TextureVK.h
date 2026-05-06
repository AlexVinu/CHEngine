#pragma once

namespace CHModules {

    class TextureVK
    {
    public:
        TextureVK(const uint8_t* data, uint32_t width, uint32_t height, uint32_t channels);
        ~TextureVK() ;

        uint32_t GetWidth()  const  { return m_Width; }
        uint32_t GetHeight() const  { return m_Height; }

    private:
        uint32_t m_Width  = 0;
        uint32_t m_Height = 0;
        // TODO: VkImage + VkDeviceMemory + VkImageView + VkSampler
    };

}
