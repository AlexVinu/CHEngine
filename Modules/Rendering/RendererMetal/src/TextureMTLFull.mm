#include "TextureMTLFull.h"
#include "MetalGlobals.h"
#include <Log/Log.h>

#import <Metal/Metal.h>

namespace CHModules {

// ─── Format mapping ───────────────────────────────────────────────────────────
uint32_t TextureMTLFull::ToMTLPixelFormat(CHEngine::TextureFormat fmt)
{
    using F = CHEngine::TextureFormat;
    switch (fmt) {
        case F::RGBA8_UNORM:      return MTLPixelFormatRGBA8Unorm;
        case F::RGBA8_UNORM_SRGB: return MTLPixelFormatRGBA8Unorm_sRGB;
        case F::RGBA16_FLOAT:     return MTLPixelFormatRGBA16Float;
        case F::R8_UNORM:         return MTLPixelFormatR8Unorm;
        case F::RG8_UNORM:        return MTLPixelFormatRG8Unorm;
        case F::R16_FLOAT:        return MTLPixelFormatR16Float;
        case F::RG16_FLOAT:       return MTLPixelFormatRG16Float;
        case F::R32_FLOAT:        return MTLPixelFormatR32Float;
        case F::RG32_FLOAT:       return MTLPixelFormatRG32Float;
        case F::RGBA32_FLOAT:     return MTLPixelFormatRGBA32Float;
        case F::D32_FLOAT:        return MTLPixelFormatDepth32Float;
        case F::D24_UNORM_S8_UINT: return MTLPixelFormatDepth32Float; // Metal no D24S8 on Apple Silicon
        case F::D32_FLOAT_S8_UINT: return MTLPixelFormatDepth32Float_Stencil8;
        default:                  return MTLPixelFormatRGBA8Unorm;
    }
}

static bool IsDepthFormat(CHEngine::TextureFormat fmt)
{
    using F = CHEngine::TextureFormat;
    return fmt == F::D32_FLOAT || fmt == F::D24_UNORM_S8_UINT || fmt == F::D32_FLOAT_S8_UINT;
}

// ─── Sampler ──────────────────────────────────────────────────────────────────
void TextureMTLFull::CreateSampler()
{
    id<MTLDevice> device = (id<MTLDevice>)MTLGlobals::g_Device;
    if (!device) return;

    MTLSamplerDescriptor* sd = [[MTLSamplerDescriptor alloc] init];
    sd.minFilter    = MTLSamplerMinMagFilterLinear;
    sd.magFilter    = MTLSamplerMinMagFilterLinear;
    sd.mipFilter    = MTLSamplerMipFilterLinear;
    sd.sAddressMode = MTLSamplerAddressModeClampToEdge;
    sd.tAddressMode = MTLSamplerAddressModeClampToEdge;
    m_Sampler = (void*)[device newSamplerStateWithDescriptor:sd];
    [sd release];
}

// ─── Constructor: raw pixel data ──────────────────────────────────────────────
TextureMTLFull::TextureMTLFull(const uint8_t* data, uint32_t width, uint32_t height,
                               uint32_t channels, uint32_t /*mips*/)
    : m_Width(width), m_Height(height), m_Format(CHEngine::TextureFormat::RGBA8_UNORM)
{
    id<MTLDevice> device = (id<MTLDevice>)MTLGlobals::g_Device;
    if (!device) { CHE_CORE_ERROR("TextureMTLFull: device is null"); return; }

    MTLPixelFormat fmt = (channels == 1) ? MTLPixelFormatR8Unorm
                       : (channels == 2) ? MTLPixelFormatRG8Unorm
                       : MTLPixelFormatRGBA8Unorm;
    uint32_t bpp = (channels <= 2) ? channels : 4;

    // Expand RGB → RGBA if needed
    std::vector<uint8_t> expanded;
    const uint8_t* src = data;
    if (channels == 3 && data) {
        expanded.resize(width * height * 4);
        for (uint32_t i = 0; i < width * height; ++i) {
            expanded[i*4+0] = data[i*3+0];
            expanded[i*4+1] = data[i*3+1];
            expanded[i*4+2] = data[i*3+2];
            expanded[i*4+3] = 255;
        }
        src = expanded.data();
        bpp = 4;
        fmt = MTLPixelFormatRGBA8Unorm;
    }

    MTLTextureDescriptor* desc = [[MTLTextureDescriptor alloc] init];
    desc.textureType      = MTLTextureType2D;
    desc.pixelFormat      = fmt;
    desc.width            = width;
    desc.height           = height;
    desc.mipmapLevelCount = 1;
    desc.usage            = MTLTextureUsageShaderRead;
    desc.storageMode      = MTLStorageModeShared;

    id<MTLTexture> tex = [device newTextureWithDescriptor:desc];
    [desc release];

    if (src && tex) {
        [tex replaceRegion:MTLRegionMake2D(0,0,width,height)
               mipmapLevel:0
                 withBytes:src
               bytesPerRow:width * bpp];
    }
    m_Texture = (void*)tex;
    CreateSampler();
}

// ─── Constructor: render target / depth ──────────────────────────────────────
TextureMTLFull::TextureMTLFull(uint32_t width, uint32_t height,
                               CHEngine::TextureFormat format,
                               CHEngine::TextureUsage  usage,
                               uint32_t /*mips*/)
    : m_Width(width), m_Height(height), m_Format(format)
{
    id<MTLDevice> device = (id<MTLDevice>)MTLGlobals::g_Device;
    if (!device) { CHE_CORE_ERROR("TextureMTLFull: device is null"); return; }

    const bool isDepth = IsDepthFormat(format);
    MTLPixelFormat mtlFmt = (MTLPixelFormat)ToMTLPixelFormat(format);

    MTLTextureDescriptor* desc = [[MTLTextureDescriptor alloc] init];
    desc.textureType      = MTLTextureType2D;
    desc.pixelFormat      = mtlFmt;
    desc.width            = width;
    desc.height           = height;
    desc.mipmapLevelCount = 1;
    desc.storageMode      = MTLStorageModePrivate;

    // Build usage flags
    MTLTextureUsage mtlUsage = 0;
    const auto u = static_cast<uint32_t>(usage);
    if (u & static_cast<uint32_t>(CHEngine::TextureUsage::ShaderResource))
        mtlUsage |= MTLTextureUsageShaderRead;
    if (u & static_cast<uint32_t>(CHEngine::TextureUsage::RenderTarget))
        mtlUsage |= MTLTextureUsageRenderTarget;
    if (u & static_cast<uint32_t>(CHEngine::TextureUsage::DepthStencil))
        mtlUsage |= MTLTextureUsageRenderTarget;
    if (mtlUsage == 0)
        mtlUsage = isDepth ? MTLTextureUsageRenderTarget
                           : (MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget);
    desc.usage = mtlUsage;

    m_Texture = (void*)[device newTextureWithDescriptor:desc];
    [desc release];

    if (!m_Texture)
        CHE_CORE_ERROR("TextureMTLFull: failed to create {}x{} render target", width, height);

    if (!isDepth)
        CreateSampler();
}

TextureMTLFull::~TextureMTLFull()
{
    if (m_Sampler) { [(id<MTLSamplerState>)m_Sampler release]; m_Sampler = nullptr; }
    if (m_Texture) { [(id<MTLTexture>)m_Texture release];      m_Texture = nullptr; }
}

void TextureMTLFull::Bind(uint32_t slot) const
{
    id<MTLRenderCommandEncoder> enc = (id<MTLRenderCommandEncoder>)MTLGlobals::g_Encoder;
    if (!enc) return;
    if (m_Texture)
        [enc setFragmentTexture:(id<MTLTexture>)m_Texture atIndex:slot];
    if (m_Sampler)
        [enc setFragmentSamplerState:(id<MTLSamplerState>)m_Sampler atIndex:slot];
}

} // namespace CHModules
