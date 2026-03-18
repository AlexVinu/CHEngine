#include <cstdint>
#include "TextureMTL.h"
#include "MetalGlobals.h"

#include <Log/Log.h>

#import <Metal/Metal.h>

namespace CHModules {

TextureMTL::TextureMTL(const uint8_t* data, uint32_t width, uint32_t height, uint32_t channels)
    : m_Width(width), m_Height(height)
{
    id<MTLDevice> device = (id<MTLDevice>)MTLGlobals::g_Device;
    if (!device) { CHE_CORE_ERROR("TextureMTL: device is null"); return; }

    // Determine pixel format
    MTLPixelFormat format;
    uint32_t bytesPerPixel;
    switch (channels) {
    case 1:  format = MTLPixelFormatR8Unorm;    bytesPerPixel = 1; break;
    case 2:  format = MTLPixelFormatRG8Unorm;   bytesPerPixel = 2; break;
    case 4:  format = MTLPixelFormatRGBA8Unorm; bytesPerPixel = 4; break;
    case 3:
    default:
        // Metal doesn't support RGB8, convert to RGBA8
        format = MTLPixelFormatRGBA8Unorm;
        bytesPerPixel = 4;
        break;
    }

    // If 3 channels, expand to 4
    uint8_t* rgbaData = nullptr;
    const uint8_t* srcData = data;
    if (channels == 3 && data) {
        rgbaData = new uint8_t[width * height * 4];
        for (uint32_t i = 0; i < width * height; ++i) {
            rgbaData[i * 4 + 0] = data[i * 3 + 0];
            rgbaData[i * 4 + 1] = data[i * 3 + 1];
            rgbaData[i * 4 + 2] = data[i * 3 + 2];
            rgbaData[i * 4 + 3] = 255;
        }
        srcData = rgbaData;
    }

    MTLTextureDescriptor* desc = [[MTLTextureDescriptor alloc] init];
    desc.textureType = MTLTextureType2D;
    desc.pixelFormat = format;
    desc.width       = width;
    desc.height      = height;
    desc.mipmapLevelCount = 1;
    desc.usage       = MTLTextureUsageShaderRead;
    desc.storageMode = MTLStorageModeShared;

    id<MTLTexture> texture = [device newTextureWithDescriptor:desc];
    [desc release];

    if (srcData) {
        MTLRegion region = MTLRegionMake2D(0, 0, width, height);
        [texture replaceRegion:region
                   mipmapLevel:0
                     withBytes:srcData
                   bytesPerRow:width * bytesPerPixel];
    }

    delete[] rgbaData;
    m_Texture = (void*)texture;

    // Create sampler
    MTLSamplerDescriptor* samplerDesc = [[MTLSamplerDescriptor alloc] init];
    samplerDesc.minFilter    = MTLSamplerMinMagFilterLinear;
    samplerDesc.magFilter    = MTLSamplerMinMagFilterLinear;
    samplerDesc.sAddressMode = MTLSamplerAddressModeRepeat;
    samplerDesc.tAddressMode = MTLSamplerAddressModeRepeat;

    id<MTLSamplerState> sampler = [device newSamplerStateWithDescriptor:samplerDesc];
    [samplerDesc release];
    m_Sampler = (void*)sampler;
}

TextureMTL::~TextureMTL()
{
    if (m_Sampler)  [(id<MTLSamplerState>)m_Sampler release];
    if (m_Texture)  [(id<MTLTexture>)m_Texture release];
}

void TextureMTL::Bind(uint32_t slot) const
{
    id<MTLRenderCommandEncoder> encoder = (id<MTLRenderCommandEncoder>)MTLGlobals::g_Encoder;
    if (!encoder) return;

    if (m_Texture)
        [encoder setFragmentTexture:(id<MTLTexture>)m_Texture atIndex:slot];
    if (m_Sampler)
        [encoder setFragmentSamplerState:(id<MTLSamplerState>)m_Sampler atIndex:0];
}

void TextureMTL::Unbind() const
{
    // Metal textures don't need explicit unbind
}

} // namespace CHModules
