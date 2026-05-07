#include "FrameGraphBackendMTL.h"
#include "RenderFactoryMTL.h"
#include "MetalGlobals.h"
#include "MetalContext.h"

#import <Metal/Metal.h>
#include <Log/Log.h>

namespace CHModules {

FrameGraphBackendMTL::FrameGraphBackendMTL(RenderFactoryMTL& factory)
    : m_Factory(factory)
{
}

// ─── Load / Store op helpers ──────────────────────────────────────────────────
static MTLLoadAction ToMTLLoadAction(CHEngine::ELoadOp op)
{
    switch (op) {
        case CHEngine::ELoadOp::Clear:    return MTLLoadActionClear;
        case CHEngine::ELoadOp::Load:     return MTLLoadActionLoad;
        case CHEngine::ELoadOp::DontCare: return MTLLoadActionDontCare;
        default:                          return MTLLoadActionClear;
    }
}

static MTLStoreAction ToMTLStoreAction(CHEngine::EStoreOp op)
{
    switch (op) {
        case CHEngine::EStoreOp::Store:    return MTLStoreActionStore;
        case CHEngine::EStoreOp::DontCare: return MTLStoreActionDontCare;
        default:                           return MTLStoreActionStore;
    }
}

// ─── Index format helper ──────────────────────────────────────────────────────
static MTLIndexType ToMTLIndexType(CHEngine::IndexFormat fmt)
{
    return (fmt == CHEngine::IndexFormat::UInt16) ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32;
}

// ─── Execute ─────────────────────────────────────────────────────────────────
void FrameGraphBackendMTL::Execute(const CHEngine::Vector<CHEngine::PassDesc>& passes)
{
    id<MTLCommandBuffer> cmdBuf = (id<MTLCommandBuffer>)MTLGlobals::g_CommandBuffer;
    if (!cmdBuf) {
        CHE_CORE_ERROR("FrameGraphBackendMTL: no command buffer");
        return;
    }

    // End the current screen encoder (started by BeginFrame) so we can
    // open per-pass encoders.
    {
        id<MTLRenderCommandEncoder> screenEnc = (id<MTLRenderCommandEncoder>)MTLGlobals::g_Encoder;
        if (screenEnc) {
            [screenEnc endEncoding];
            [screenEnc release];
            MTLGlobals::g_Encoder = nullptr;
        }
    }

    for (const CHEngine::PassDesc& pass : passes)
    {
        // ── Build MTLRenderPassDescriptor ────────────────────────────────────
        MTLRenderPassDescriptor* rpDesc = [MTLRenderPassDescriptor renderPassDescriptor];

        // Color attachments
        for (uint32_t i = 0; i < static_cast<uint32_t>(pass.ColorAttachments.size()); ++i)
        {
            CHEngine::TextureHandle th = pass.ColorAttachments[i];
            TextureMTLFull* tex = m_Factory.Textures.Get(th);
            if (!tex) continue;

            rpDesc.colorAttachments[i].texture     = (id<MTLTexture>)tex->GetNativeTexture();
            rpDesc.colorAttachments[i].loadAction  = ToMTLLoadAction(pass.ColorLoadOp);
            rpDesc.colorAttachments[i].storeAction = ToMTLStoreAction(pass.ColorStoreOp);
            rpDesc.colorAttachments[i].clearColor  = MTLClearColorMake(
                pass.ClearColor.r, pass.ClearColor.g, pass.ClearColor.b, pass.ClearColor.a);
        }

        // Depth attachment
        if (pass.DepthAttachment.IsValid())
        {
            TextureMTLFull* depthTex = m_Factory.Textures.Get(pass.DepthAttachment);
            if (depthTex) {
                rpDesc.depthAttachment.texture     = (id<MTLTexture>)depthTex->GetNativeTexture();
                rpDesc.depthAttachment.loadAction  = ToMTLLoadAction(pass.DepthLoadOp);
                rpDesc.depthAttachment.storeAction = MTLStoreActionDontCare;
                rpDesc.depthAttachment.clearDepth  = pass.ClearDepth;
            }
        }

        // ── Open encoder ─────────────────────────────────────────────────────
        id<MTLRenderCommandEncoder> enc =
            [cmdBuf renderCommandEncoderWithDescriptor:rpDesc];
        if (!enc) {
            CHE_CORE_WARN("FrameGraphBackendMTL: failed to create encoder for pass '{}'", pass.Name.c_str());
            continue;
        }
        [enc retain];
        MTLGlobals::g_Encoder = (void*)enc;

        // Label for GPU profiler
        if (!pass.Name.empty())
            enc.label = [NSString stringWithUTF8String:pass.Name.c_str()];

        // ── Viewport ─────────────────────────────────────────────────────────
        if (pass.ViewportWidth > 0 && pass.ViewportHeight > 0) {
            MTLViewport vp;
            vp.originX = 0.0; vp.originY = 0.0;
            vp.width   = pass.ViewportWidth;
            vp.height  = pass.ViewportHeight;
            vp.znear   = 0.0; vp.zfar = 1.0;
            [enc setViewport:vp];
        }

        // ── Apply pipeline ────────────────────────────────────────────────────
        if (pass.Pipeline.IsValid())
        {
            PipelineMTL* pipeline = m_Factory.Pipelines.Get(pass.Pipeline);
            if (pipeline) {
                if (pipeline->GetPSO())
                    [enc setRenderPipelineState:(id<MTLRenderPipelineState>)pipeline->GetPSO()];
                if (pipeline->GetDepthState())
                    [enc setDepthStencilState:(id<MTLDepthStencilState>)pipeline->GetDepthState()];
            }
        }

        // ── Pass-level uniforms ───────────────────────────────────────────────
        for (const CHEngine::UniformBinding& ub : pass.Uniforms)
        {
            UnifiedBufferMTL* buf = m_Factory.Buffers.Get(ub.Buffer);
            if (!buf) continue;

            id<MTLBuffer> mtlBuf = (id<MTLBuffer>)buf->GetNative();
            NSUInteger offset    = (NSUInteger)ub.Offset;
            uint32_t   slot      = ub.Slot;

            [enc setVertexBuffer:mtlBuf   offset:offset atIndex:slot];
            [enc setFragmentBuffer:mtlBuf offset:offset atIndex:slot];
        }

        // ── Pass-level textures ───────────────────────────────────────────────
        for (const CHEngine::TextureBinding& tb : pass.Textures)
        {
            TextureMTLFull* tex = m_Factory.Textures.Get(tb.Texture);
            if (!tex) continue;
            if (tex->GetNativeTexture())
                [enc setFragmentTexture:(id<MTLTexture>)tex->GetNativeTexture() atIndex:tb.Slot];
            if (tex->GetNativeSampler())
                [enc setFragmentSamplerState:(id<MTLSamplerState>)tex->GetNativeSampler() atIndex:tb.Slot];
        }

        // ── Draw calls ────────────────────────────────────────────────────────
        if (pass.Fullscreen) {
            // Fullscreen triangle (no vertex buffer needed)
            [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        } else {
            for (const CHEngine::DrawDesc& draw : pass.Draws)
            {
                // Vertex buffer at slot 10 (above UBO slots 0–3)
                UnifiedBufferMTL* vb = m_Factory.Buffers.Get(draw.VertexBuffer);
                if (vb)
                    [enc setVertexBuffer:(id<MTLBuffer>)vb->GetNative() offset:0 atIndex:10];

                // Index buffer
                UnifiedBufferMTL* ib = m_Factory.Buffers.Get(draw.IndexBuffer);
                if (!ib) continue;

                // Per-draw uniforms (Object UBO ring)
                for (const CHEngine::UniformBinding& ub : draw.Uniforms)
                {
                    UnifiedBufferMTL* buf = m_Factory.Buffers.Get(ub.Buffer);
                    if (!buf) continue;
                    id<MTLBuffer> mtlBuf = (id<MTLBuffer>)buf->GetNative();
                    NSUInteger    offset  = (NSUInteger)ub.Offset;
                    uint32_t      slot    = ub.Slot;
                    [enc setVertexBuffer:mtlBuf   offset:offset atIndex:slot];
                    [enc setFragmentBuffer:mtlBuf offset:offset atIndex:slot];
                }

                // Per-draw textures
                for (const CHEngine::TextureBinding& tb : draw.Textures)
                {
                    TextureMTLFull* tex = m_Factory.Textures.Get(tb.Texture);
                    if (!tex) continue;
                    if (tex->GetNativeTexture())
                        [enc setFragmentTexture:(id<MTLTexture>)tex->GetNativeTexture() atIndex:tb.Slot];
                    if (tex->GetNativeSampler())
                        [enc setFragmentSamplerState:(id<MTLSamplerState>)tex->GetNativeSampler() atIndex:tb.Slot];
                }

                // Draw indexed
                MTLIndexType idxType = ToMTLIndexType(draw.IdxFormat);
                NSUInteger   idxSize = (idxType == MTLIndexTypeUInt16) ? 2 : 4;

                [enc drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                               indexCount:draw.IndexCount
                                indexType:idxType
                              indexBuffer:(id<MTLBuffer>)ib->GetNative()
                        indexBufferOffset:(NSUInteger)draw.FirstIndex * idxSize
                            instanceCount:draw.InstanceCount
                               baseVertex:draw.BaseVertex
                             baseInstance:0];
            }
        }

        // ── End encoder ───────────────────────────────────────────────────────
        [enc endEncoding];
        [enc release];
        MTLGlobals::g_Encoder = nullptr;
    }

    // Restore screen encoder for ImGui (MTLLoadActionLoad to preserve content)
    {
        MTLRenderPassDescriptor* screenRPDesc =
            (MTLRenderPassDescriptor*)MTLGlobals::g_ScreenRPDesc;
        if (screenRPDesc) {
            // Switch to Load so we don't clear what the framegraph drew
            screenRPDesc.colorAttachments[0].loadAction = MTLLoadActionLoad;
            if (screenRPDesc.depthAttachment.texture)
                screenRPDesc.depthAttachment.loadAction = MTLLoadActionLoad;

            id<MTLRenderCommandEncoder> screenEnc =
                [cmdBuf renderCommandEncoderWithDescriptor:screenRPDesc];
            if (screenEnc) {
                [screenEnc retain];
                MTLGlobals::g_Encoder = (void*)screenEnc;

                // Restore viewport to full screen
                if (MTLGlobals::g_Width > 0 && MTLGlobals::g_Height > 0) {
                    MTLViewport vp;
                    vp.originX = 0.0; vp.originY = 0.0;
                    vp.width   = MTLGlobals::g_Width;
                    vp.height  = MTLGlobals::g_Height;
                    vp.znear   = 0.0; vp.zfar = 1.0;
                    [screenEnc setViewport:vp];
                }
            }
        }
    }
}

} // namespace CHModules
