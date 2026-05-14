#include <cstdint>
#include "MetalContext.h"
#include "MetalGlobals.h"

#include <Log/Log.h>

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Cocoa/Cocoa.h>

namespace CHModules {

    void MetalContext::CreateDepthTexture()
    {
        id<MTLDevice> device = (id<MTLDevice>)m_Device;
        if (!device || m_Width == 0 || m_Height == 0) return;

        if (m_DepthTexture) {
            [(id<MTLTexture>)m_DepthTexture release];
            m_DepthTexture = nullptr;
        }

        MTLTextureDescriptor* desc = [[MTLTextureDescriptor alloc] init];
        desc.textureType = MTLTextureType2D;
        desc.pixelFormat = MTLPixelFormatDepth32Float;
        desc.width       = m_Width;
        desc.height      = m_Height;
        desc.usage       = MTLTextureUsageRenderTarget;
        desc.storageMode = MTLStorageModePrivate;

        id<MTLTexture> depthTex = [device newTextureWithDescriptor:desc];
        [desc release];
        m_DepthTexture = (void*)depthTex;
        MTLGlobals::g_DepthTexture = m_DepthTexture;
    }

    bool MetalContext::Init(void* nsWindow, uint32_t width, uint32_t height)
    {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (!device) {
            CHE_CORE_CRITICAL("MetalContext: MTLCreateSystemDefaultDevice() returned nil");
            return false;
        }
        // MTLCreateSystemDefaultDevice следует Create Rule → возвращает +1 объект.
        // Дополнительный retain не нужен.
        m_Device = (void*)device;
        m_NSWindow = nsWindow;
        MTLGlobals::g_Device = m_Device;
        MTLGlobals::g_ContextPtr = this;

        CHE_CORE_INFO("Metal GPU: {}", [[device name] UTF8String]);

        NSWindow* window = (NSWindow*)nsWindow;
        NSView* view = [window contentView];

        // Retina: use pixel dimensions, not screen points
        CGFloat scale = [window backingScaleFactor];
        m_Width  = static_cast<uint32_t>(width  * scale);
        m_Height = static_cast<uint32_t>(height * scale);

        CAMetalLayer* metalLayer = [CAMetalLayer layer];
        metalLayer.device = device;
        metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        metalLayer.drawableSize = CGSizeMake(m_Width, m_Height);
        metalLayer.contentsScale = scale;
        metalLayer.framebufferOnly = NO; // ImGui needs shader read

        [view setWantsLayer:YES];
        [view setLayer:metalLayer];
        [metalLayer retain];
        m_MetalLayer = (void*)metalLayer;

        id<MTLCommandQueue> commandQueue = [device newCommandQueue];
        m_CommandQueue = (void*)commandQueue;

        MTLRenderPassDescriptor* rpDesc = [MTLRenderPassDescriptor renderPassDescriptor];
        rpDesc.colorAttachments[0].loadAction  = MTLLoadActionClear;
        rpDesc.colorAttachments[0].storeAction = MTLStoreActionStore;
        rpDesc.colorAttachments[0].clearColor  = MTLClearColorMake(m_ClearR, m_ClearG, m_ClearB, m_ClearA);
        [rpDesc retain];
        m_RenderPassDescriptor = (void*)rpDesc;

        MTLGlobals::g_ScreenRPDesc = m_RenderPassDescriptor;
        MTLGlobals::g_Width  = m_Width;
        MTLGlobals::g_Height = m_Height;
        MTLGlobals::g_ColorPixelFormat = (uint32_t)MTLPixelFormatBGRA8Unorm;

        // Depth buffer
        CreateDepthTexture();
        if (m_DepthTexture) {
            rpDesc.depthAttachment.texture    = (id<MTLTexture>)m_DepthTexture;
            rpDesc.depthAttachment.loadAction  = MTLLoadActionClear;
            rpDesc.depthAttachment.storeAction = MTLStoreActionDontCare;
            rpDesc.depthAttachment.clearDepth  = 1.0;
            MTLGlobals::g_DepthPixelFormat = (uint32_t)MTLPixelFormatDepth32Float;
        }

        CHE_CORE_INFO("Metal initialized successfully ({}x{}, scale={})", m_Width, m_Height, (float)scale);
        return true;
    }

    void MetalContext::Shutdown()
    {
        if (m_DepthTexture)         { [(id<MTLTexture>)m_DepthTexture release];                  m_DepthTexture = nullptr; }
        if (m_RenderPassDescriptor) { [(MTLRenderPassDescriptor*)m_RenderPassDescriptor release]; m_RenderPassDescriptor = nullptr; }
        if (m_CommandQueue)         { [(id<MTLCommandQueue>)m_CommandQueue release];              m_CommandQueue = nullptr; }
        if (m_MetalLayer)           { [(CAMetalLayer*)m_MetalLayer release];                      m_MetalLayer = nullptr; }
        if (m_Device)               { [(id<MTLDevice>)m_Device release];                         m_Device = nullptr; }

        MTLGlobals::g_Device       = nullptr;
        MTLGlobals::g_DepthTexture = nullptr;
        MTLGlobals::g_ScreenRPDesc = nullptr;
        MTLGlobals::g_ContextPtr   = nullptr;
    }

    bool MetalContext::BeginFrame()
    {
        // Sync CAMetalLayer size directly from NSWindow every frame.
        // This handles macOS native fullscreen (Space fullscreen via green button)
        // where the GLFW window size callback may never fire.
        if (m_NSWindow && m_MetalLayer) {
            NSWindow* ns = (NSWindow*)m_NSWindow;
            CGFloat scale = ns.backingScaleFactor;
            NSSize contentSize = ns.contentView.bounds.size;
            uint32_t newW = static_cast<uint32_t>(contentSize.width  * scale);
            uint32_t newH = static_cast<uint32_t>(contentSize.height * scale);
            if (newW > 0 && newH > 0 && (newW != m_Width || newH != m_Height)) {
                m_Width  = newW;
                m_Height = newH;
                CAMetalLayer* l = (CAMetalLayer*)m_MetalLayer;
                l.drawableSize  = CGSizeMake(m_Width, m_Height);
                l.contentsScale = scale;
                MTLGlobals::g_Width  = m_Width;
                MTLGlobals::g_Height = m_Height;
                CreateDepthTexture();
            }
        }

        @autoreleasepool {
            CAMetalLayer* layer = (CAMetalLayer*)m_MetalLayer;
            id<CAMetalDrawable> drawable = [layer nextDrawable];
            if (!drawable) return false;
            [drawable retain];
            m_CurrentDrawable = (void*)drawable;
            MTLGlobals::g_ScreenDrawable = m_CurrentDrawable;

            MTLRenderPassDescriptor* rpDesc = (MTLRenderPassDescriptor*)m_RenderPassDescriptor;
            rpDesc.colorAttachments[0].texture    = drawable.texture;
            rpDesc.colorAttachments[0].loadAction = MTLLoadActionClear;
            rpDesc.colorAttachments[0].clearColor = MTLClearColorMake(m_ClearR, m_ClearG, m_ClearB, m_ClearA);

            if (m_DepthTexture) {
                rpDesc.depthAttachment.texture    = (id<MTLTexture>)m_DepthTexture;
                rpDesc.depthAttachment.loadAction  = MTLLoadActionClear;
                rpDesc.depthAttachment.clearDepth  = 1.0;
            }

            id<MTLCommandQueue> queue = (id<MTLCommandQueue>)m_CommandQueue;
            id<MTLCommandBuffer> cmdBuf = [queue commandBuffer];
            [cmdBuf retain];
            m_CommandBuffer = (void*)cmdBuf;
            MTLGlobals::g_CommandBuffer = m_CommandBuffer;

            id<MTLRenderCommandEncoder> encoder = [cmdBuf renderCommandEncoderWithDescriptor:rpDesc];
            [encoder retain];
            m_RenderEncoder = (void*)encoder;
            MTLGlobals::g_Encoder = m_RenderEncoder;

            MTLGlobals::g_ColorPixelFormat = (uint32_t)MTLPixelFormatBGRA8Unorm;
            MTLGlobals::g_DepthPixelFormat = m_DepthTexture ?
                (uint32_t)MTLPixelFormatDepth32Float : 0;

            MTLViewport viewport = {0.0, 0.0, (double)m_Width, (double)m_Height, 0.0, 1.0};
            [encoder setViewport:viewport];
            [encoder setFrontFacingWinding:MTLWindingCounterClockwise];
            [encoder setCullMode:MTLCullModeNone];
        }
        return true;
    }

    void MetalContext::EndFrame()
    {
        @autoreleasepool {
            id<MTLRenderCommandEncoder> encoder = (id<MTLRenderCommandEncoder>)MTLGlobals::g_Encoder;
            if (encoder) {
                [encoder endEncoding];
                [encoder release];
            }
            m_RenderEncoder = nullptr;
            MTLGlobals::g_Encoder = nullptr;

            id<MTLCommandBuffer> cmdBuf = (id<MTLCommandBuffer>)m_CommandBuffer;
            id<CAMetalDrawable> drawable = (id<CAMetalDrawable>)m_CurrentDrawable;

            if (cmdBuf && drawable) {
                [cmdBuf presentDrawable:drawable];
                [cmdBuf commit];
            }

            if (cmdBuf)    [cmdBuf release];
            if (drawable)  [drawable release];

            m_CommandBuffer   = nullptr;
            m_CurrentDrawable = nullptr;
            MTLGlobals::g_CommandBuffer  = nullptr;
            MTLGlobals::g_ScreenDrawable = nullptr;
        }
    }

    void MetalContext::SetViewport(uint32_t width, uint32_t height)
    {
        // width/height come as screen points from GLFW resize callback
        CGFloat scale = 1.0;
        if (m_NSWindow)
            scale = [(NSWindow*)m_NSWindow backingScaleFactor];

        m_Width  = static_cast<uint32_t>(width  * scale);
        m_Height = static_cast<uint32_t>(height * scale);

        CAMetalLayer* layer = (CAMetalLayer*)m_MetalLayer;
        layer.drawableSize = CGSizeMake(m_Width, m_Height);
        layer.contentsScale = scale;

        MTLGlobals::g_Width  = m_Width;
        MTLGlobals::g_Height = m_Height;

        CreateDepthTexture();

        MTLRenderPassDescriptor* rpDesc = (MTLRenderPassDescriptor*)m_RenderPassDescriptor;
        if (m_DepthTexture && rpDesc) {
            rpDesc.depthAttachment.texture = (id<MTLTexture>)m_DepthTexture;
        }
    }

    void MetalContext::SetClearColor(float r, float g, float b, float a)
    {
        m_ClearR = r; m_ClearG = g; m_ClearB = b; m_ClearA = a;
    }

}
