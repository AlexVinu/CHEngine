#pragma once

#include "Render/ITexture.h"

namespace CHModules {

    class TextureVK : public CHEngine::ITexture
    {
    public:
        TextureVK(const uint8_t* data, uint32_t width, uint32_t height, uint32_t channels);
        ~TextureVK() override;

        void Bind(uint32_t slot = 0) const override;
        void Unbind() const override;

        uint32_t GetWidth()  const override { return m_Width; }
        uint32_t GetHeight() const override { return m_Height; }

    private:
        uint32_t m_Width  = 0;
        uint32_t m_Height = 0;
        // TODO: VkImage + VkDeviceMemory + VkImageView + VkSampler
    };

}
