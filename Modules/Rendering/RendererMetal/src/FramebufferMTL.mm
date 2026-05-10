#include <cstdint>
#include "FramebufferMTL.h"
#include "MetalGlobals.h"

#include <Log/Log.h>
#include <algorithm>

#import <Metal/Metal.h>

namespace CHModules {

FramebufferMTL::FramebufferMTL(uint32_t width, uint32_t height)
    : m_Width(std::max(width, 1u)), m_Height(std::max(height, 1u))
{
    CreateAttachments();
}

FramebufferMTL::~FramebufferMTL()
{
    DestroyAttachments();
}

void FramebufferMTL::CreateAttachments()
{
    id<MTLDevice> device = (id<MTLDevice>)MTLGlobals::g_Device;
    if (!device) {
        CHE_CORE_ERROR("FramebufferMTL: device is null");
        return;
    }

    // Color texture (RGBA8Unorm — for ImGui::Image compatibility)
    MTLTextureDescriptor* colorDesc = [[MTLTextureDescriptor alloc] init];
    colorDesc.textureType = MTLTextureType2D;
    colorDesc.pixelFormat = MTLPixelFormatRGBA8Unorm;
    colorDesc.width       = m_Width;
    colorDesc.height      = m_Height;
    colorDesc.usage       = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    colorDesc.storageMode = MTLStorageModePrivate;

    id<MTLTexture> colorTex = [device newTextureWithDescriptor:colorDesc];
    [colorDesc release];
    m_ColorTexture = (void*)colorTex;

    // Depth texture (Depth32Float)
    MTLTextureDescriptor* depthDesc = [[MTLTextureDescriptor alloc] init];
    depthDesc.textureType = MTLTextureType2D;
    depthDesc.pixelFormat = MTLPixelFormatDepth32Float;
    depthDesc.width       = m_Width;
    depthDesc.height      = m_Height;
    depthDesc.usage       = MTLTextureUsageRenderTarget;
    depthDesc.storageMode = MTLStorageModePrivate;

    id<MTLTexture> depthTex = [device newTextureWithDescriptor:depthDesc];
    [depthDesc release];
    m_DepthTexture = (void*)depthTex;

    // Render pass descriptor for this FBO
    MTLRenderPassDescriptor* rpDesc = [MTLRenderPassDescriptor renderPassDescriptor];
    rpDesc.colorAttachments[0].texture    = colorTex;
    rpDesc.colorAttachments[0].loadAction  = MTLLoadActionClear;
    rpDesc.colorAttachments[0].storeAction = MTLStoreActionStore;
    rpDesc.colorAttachments[0].clearColor  = MTLClearColorMake(0.18, 0.18, 0.20, 1.0);

    rpDesc.depthAttachment.texture    = depthTex;
    rpDesc.depthAttachment.loadAction  = MTLLoadActionClear;
    rpDesc.depthAttachment.storeAction = MTLStoreActionDontCare;
    rpDesc.depthAttachment.clearDepth  = 1.0;

    [rpDesc retain];
    m_RPDesc = (void*)rpDesc;
}

void FramebufferMTL::DestroyAttachments()
{
    if (m_RPDesc)       { [(MTLRenderPassDescriptor*)m_RPDesc release]; m_RPDesc = nullptr; }
    if (m_DepthTexture) { [(id<MTLTexture>)m_DepthTexture release];    m_DepthTexture = nullptr; }
    if (m_ColorTexture) { [(id<MTLTexture>)m_ColorTexture release];    m_ColorTexture = nullptr; }
}

void FramebufferMTL::Bind() const
{
    // End current screen encoder, start FBO encoder
    id<MTLRenderCommandEncoder> currentEncoder = (id<MTLRenderCommandEncoder>)MTLGlobals::g_Encoder;
    if (currentEncoder) {
        [currentEncoder endEncoding];
        [currentEncoder release];
        MTLGlobals::g_Encoder = nullptr;
    }

    id<MTLCommandBuffer> cmdBuf = (id<MTLCommandBuffer>)MTLGlobals::g_CommandBuffer;
    if (!cmdBuf || !m_RPDesc) return;

    MTLRenderPassDescriptor* rpDesc = (MTLRenderPassDescriptor*)m_RPDesc;
    id<MTLRenderCommandEncoder> encoder = [cmdBuf renderCommandEncoderWithDescriptor:rpDesc];
    [encoder retain];
    MTLGlobals::g_Encoder = (void*)encoder;

    // Set viewport and depth stencil state for FBO
    MTLViewport vp = {0, 0, (double)m_Width, (double)m_Height, 0.0, 1.0};
    [encoder setViewport:vp];
    [encoder setFrontFacingWinding:MTLWindingCounterClockwise];
    [encoder setCullMode:MTLCullModeNone];

    // Update globals for FBO rendering
    MTLGlobals::g_ColorPixelFormat = (uint32_t)MTLPixelFormatRGBA8Unorm;
    MTLGlobals::g_DepthPixelFormat = (uint32_t)MTLPixelFormatDepth32Float;

    // Create and set depth stencil state
    id<MTLDevice> device = (id<MTLDevice>)MTLGlobals::g_Device;
    MTLDepthStencilDescriptor* depthDesc = [[MTLDepthStencilDescriptor alloc] init];
    depthDesc.depthCompareFunction = MTLCompareFunctionLess;
    depthDesc.depthWriteEnabled = MTLGlobals::g_DepthWriteEnabled;
    id<MTLDepthStencilState> depthState = [device newDepthStencilStateWithDescriptor:depthDesc];
    [encoder setDepthStencilState:depthState];
    [depthDesc release];
    [depthState release];
}

void FramebufferMTL::Unbind() const
{
    // End FBO encoder, start new screen encoder
    id<MTLRenderCommandEncoder> fboEncoder = (id<MTLRenderCommandEncoder>)MTLGlobals::g_Encoder;
    if (fboEncoder) {
        [fboEncoder endEncoding];
        [fboEncoder release];
        MTLGlobals::g_Encoder = nullptr;
    }

    id<MTLCommandBuffer> cmdBuf = (id<MTLCommandBuffer>)MTLGlobals::g_CommandBuffer;
    MTLRenderPassDescriptor* screenRPDesc = (MTLRenderPassDescriptor*)MTLGlobals::g_ScreenRPDesc;
    if (!cmdBuf || !screenRPDesc) return;

    // Re-create screen encoder (don't clear — loadAction is now Load)
    screenRPDesc.colorAttachments[0].loadAction = MTLLoadActionLoad;
    if (MTLGlobals::g_DepthTexture)
        screenRPDesc.depthAttachment.loadAction = MTLLoadActionLoad;

    id<MTLRenderCommandEncoder> encoder = [cmdBuf renderCommandEncoderWithDescriptor:screenRPDesc];
    [encoder retain];
    MTLGlobals::g_Encoder = (void*)encoder;

    MTLViewport vp = {0, 0, (double)MTLGlobals::g_Width, (double)MTLGlobals::g_Height, 0.0, 1.0};
    [encoder setViewport:vp];
    [encoder setFrontFacingWinding:MTLWindingCounterClockwise];
    [encoder setCullMode:MTLCullModeNone];

    // Restore screen pixel formats
    MTLGlobals::g_ColorPixelFormat = (uint32_t)MTLPixelFormatBGRA8Unorm;
    MTLGlobals::g_DepthPixelFormat = MTLGlobals::g_DepthTexture ?
        (uint32_t)MTLPixelFormatDepth32Float : 0;
}

void FramebufferMTL::Resize(uint32_t w, uint32_t h)
{
    w = std::max(w, 1u);
    h = std::max(h, 1u);
    if (m_Width == w && m_Height == h) return;

    m_Width  = w;
    m_Height = h;

    DestroyAttachments();
    CreateAttachments();
    // Не очищаем текстуры здесь — Resize вызывается из OnImGuiRender,
    // когда ImGui уже кеширует encoder. Очистка происходит автоматически
    // при следующем Bind() через MTLLoadActionClear в rpDesc.
}

uint32_t FramebufferMTL::GetColorAttachmentID() const
{
    // For Metal, return the pointer cast to uintptr_t (used as ImTextureID)
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(m_ColorTexture));
}

} // namespace CHModules
