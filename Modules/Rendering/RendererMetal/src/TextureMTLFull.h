#pragma once

#include "Render/Core/RenderTypes.h"
#include <cstdint>

namespace CHModules {

    /// Extended Metal texture that supports creation from TextureFormat
    /// (for render targets, depth buffers, etc.) as well as raw pixel data.
    class TextureMTLFull {
    public:
        /// Create from raw pixel data (for asset textures).
        TextureMTLFull(const uint8_t* data, uint32_t width, uint32_t height,
                       uint32_t channels, uint32_t mips = 1);

        /// Create from format descriptor (for render targets / depth buffers).
        TextureMTLFull(uint32_t width, uint32_t height,
                       CHEngine::TextureFormat format,
                       CHEngine::TextureUsage  usage,
                       uint32_t mips = 1);

        ~TextureMTLFull();

        TextureMTLFull(const TextureMTLFull&)            = delete;
        TextureMTLFull& operator=(const TextureMTLFull&) = delete;

        /// Bind as fragment shader texture at the given slot.
        void Bind(uint32_t slot = 0) const;

        void*    GetNativeTexture() const { return m_Texture; }
        void*    GetNativeSampler() const { return m_Sampler; }
        uint32_t GetWidth()  const { return m_Width; }
        uint32_t GetHeight() const { return m_Height; }
        CHEngine::TextureFormat GetFormat() const { return m_Format; }

    private:
        void CreateSampler();
        static uint32_t ToMTLPixelFormat(CHEngine::TextureFormat fmt);

        void* m_Texture = nullptr; // id<MTLTexture>
        void* m_Sampler = nullptr; // id<MTLSamplerState>
        uint32_t m_Width  = 0;
        uint32_t m_Height = 0;
        CHEngine::TextureFormat m_Format = CHEngine::TextureFormat::RGBA8_UNORM;
    };

} // namespace CHModules
