#pragma once

#include "Render/ITexture.h"

namespace CHModules {
    class TextureMTL : public CHEngine::ITexture
    {
    public:
        TextureMTL(const uint8_t* data, uint32_t width, uint32_t height, uint32_t channels);
        ~TextureMTL() override;

        void Bind(uint32_t slot = 0) const override;
        void Unbind() const override;

        uint32_t GetWidth()  const override { return m_Width; }
        uint32_t GetHeight() const override { return m_Height; }

        void* GetNativeTexture() const { return m_Texture; }

    private:
        void* m_Texture   = nullptr; // id<MTLTexture>
        void* m_Sampler   = nullptr; // id<MTLSamplerState>
        uint32_t m_Width  = 0;
        uint32_t m_Height = 0;
    };
}
