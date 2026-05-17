#include "FrameGraphBackendMTL.h"
#include "RenderFactoryMTL.h"
#include "MetalGlobals.h"

#import <Metal/Metal.h>
#include <Log/Log.h>
#include "CHEngine/Application.h"

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
void FrameGraphBackendMTL::Execute(const Vector<CHEngine::PassDesc>& passes)
{
    @autoreleasepool {

    id<MTLCommandBuffer> cmdBuf = (id<MTLCommandBuffer>)MTLGlobals::g_CommandBuffer;
    if (!cmdBuf) {
        CHE_CORE_ERROR("FrameGraphBackendMTL: no command buffer — was BeginFrame called?");
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

    CHE_CORE_INFO("FrameGraphBackendMTL: executing {} pass(es)", passes.size());

    for (size_t passIdx = 0; passIdx < passes.size(); ++passIdx)
    {
        const CHEngine::PassDesc& pass = passes[passIdx];
        CHE_CORE_INFO("FrameGraphBackendMTL: pass[{}] = '{}'", passIdx, pass.Name.c_str());

        // ── Build MTLRenderPassDescriptor ────────────────────────────────────
        MTLRenderPassDescriptor* rpDesc = [MTLRenderPassDescriptor renderPassDescriptor];
        if (!rpDesc) {
            CHE_CORE_ERROR("FrameGraphBackendMTL: failed to create render pass descriptor");
            continue;
        }

        // Color attachments
        bool hasValidColor = false;
        for (uint32_t i = 0; i < static_cast<uint32_t>(pass.ColorAttachments.size()); ++i)
        {
            CHEngine::TextureHandle th = pass.ColorAttachments[i];
            if (!th.IsValid()) continue;
            TextureMTLFull* tex = m_Factory.Textures.Get(th);
            if (!tex || !tex->GetNativeTexture()) {
                CHE_CORE_WARN("FrameGraphBackendMTL: pass '{}' color attachment {} is null", pass.Name.c_str(), i);
                continue;
            }

            rpDesc.colorAttachments[i].texture     = (id<MTLTexture>)tex->GetNativeTexture();
            rpDesc.colorAttachments[i].loadAction  = ToMTLLoadAction(pass.ColorLoadOp);
            rpDesc.colorAttachments[i].storeAction = ToMTLStoreAction(pass.ColorStoreOp);
            rpDesc.colorAttachments[i].clearColor  = MTLClearColorMake(
                pass.ClearColor.r, pass.ClearColor.g, pass.ClearColor.b, pass.ClearColor.a);
            hasValidColor = true;
        }

        // Depth attachment
        if (pass.DepthAttachment.IsValid())
        {
            TextureMTLFull* depthTex = m_Factory.Textures.Get(pass.DepthAttachment);
            if (depthTex && depthTex->GetNativeTexture()) {
                rpDesc.depthAttachment.texture     = (id<MTLTexture>)depthTex->GetNativeTexture();
                rpDesc.depthAttachment.loadAction  = ToMTLLoadAction(pass.DepthLoadOp);
                // Always store depth: multiple passes may read depth from previous pass
                // (e.g. sphere impostor pass reads cube depth from MainColorPass).
                // DontCare would discard depth between passes causing objects to show
                // through each other incorrectly.
                rpDesc.depthAttachment.storeAction = MTLStoreActionStore;
                rpDesc.depthAttachment.clearDepth  = pass.ClearDepth;
            } else {
                CHE_CORE_WARN("FrameGraphBackendMTL: pass '{}' depth attachment is null", pass.Name.c_str());
            }
        }

        // For fullscreen passes without color attachments, use the screen pass descriptor
        if (!hasValidColor && pass.Fullscreen && !pass.ColorAttachments.empty()) {
            CHE_CORE_WARN("FrameGraphBackendMTL: fullscreen pass '{}' has no valid color target — skipping", pass.Name.c_str());
            continue;
        }

        // ── Open encoder ─────────────────────────────────────────────────────
        id<MTLRenderCommandEncoder> enc =
            [cmdBuf renderCommandEncoderWithDescriptor:rpDesc];
        if (!enc) {
            CHE_CORE_WARN("FrameGraphBackendMTL: could not create encoder for pass '{}'", pass.Name.c_str());
            continue;
        }
        [enc retain];
        MTLGlobals::g_Encoder = (void*)enc;

        if (!pass.Name.empty())
            enc.label = [NSString stringWithUTF8String:pass.Name.c_str()];

        // ── Viewport ─────────────────────────────────────────────────────────
        if (pass.ViewportWidth > 0 && pass.ViewportHeight > 0) {
            MTLViewport vp = { 0.0, 0.0, (double)pass.ViewportWidth, (double)pass.ViewportHeight, 0.0, 1.0 };
            [enc setViewport:vp];
        }

        // ── Apply pipeline ────────────────────────────────────────────────────
        if (pass.Pipeline.IsValid())
        {
            PipelineMTL* pipeline = m_Factory.Pipelines.Get(pass.Pipeline);
            if (pipeline) {
                if (pipeline->GetPSO())
                    [enc setRenderPipelineState:(id<MTLRenderPipelineState>)pipeline->GetPSO()];
                else
                    CHE_CORE_WARN("FrameGraphBackendMTL: pass '{}' PSO is null", pass.Name.c_str());
                if (pipeline->GetDepthState())
                    [enc setDepthStencilState:(id<MTLDepthStencilState>)pipeline->GetDepthState()];
            }
        }

        // ── Pass-level uniforms ───────────────────────────────────────────────
        for (const CHEngine::UniformBinding& ub : pass.Uniforms)
        {
            if (!ub.Buffer.IsValid()) continue;
            UnifiedBufferMTL* buf = m_Factory.Buffers.Get(ub.Buffer);
            if (!buf || !buf->GetNative()) continue;

            id<MTLBuffer> mtlBuf = (id<MTLBuffer>)buf->GetNative();
            [enc setVertexBuffer:mtlBuf   offset:(NSUInteger)ub.Offset atIndex:ub.Slot];
            [enc setFragmentBuffer:mtlBuf offset:(NSUInteger)ub.Offset atIndex:ub.Slot];
        }

        // ── Pass-level textures ───────────────────────────────────────────────
        for (const CHEngine::TextureBinding& tb : pass.Textures)
        {
            if (!tb.Texture.IsValid()) continue;
            TextureMTLFull* tex = m_Factory.Textures.Get(tb.Texture);
            if (!tex) continue;
            if (tex->GetNativeTexture())
                [enc setFragmentTexture:(id<MTLTexture>)tex->GetNativeTexture() atIndex:tb.Slot];
            if (tex->GetNativeSampler())
                [enc setFragmentSamplerState:(id<MTLSamplerState>)tex->GetNativeSampler() atIndex:tb.Slot];
        }

        // ── Draw calls ────────────────────────────────────────────────────────
        if (pass.Fullscreen) {
            [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        } else {
            for (const CHEngine::DrawDesc& draw : pass.Draws)
            {
                if (!draw.VertexBuffer.IsValid() || !draw.IndexBuffer.IsValid()) continue;

                UnifiedBufferMTL* vb = m_Factory.Buffers.Get(draw.VertexBuffer);
                UnifiedBufferMTL* ib = m_Factory.Buffers.Get(draw.IndexBuffer);
                if (!vb || !ib) continue;

                [enc setVertexBuffer:(id<MTLBuffer>)vb->GetNative() offset:0 atIndex:10];

                for (const CHEngine::UniformBinding& ub : draw.Uniforms)
                {
                    if (!ub.Buffer.IsValid()) continue;
                    UnifiedBufferMTL* ubuf = m_Factory.Buffers.Get(ub.Buffer);
                    if (!ubuf || !ubuf->GetNative()) continue;
                    id<MTLBuffer> mtlBuf = (id<MTLBuffer>)ubuf->GetNative();
                    [enc setVertexBuffer:mtlBuf   offset:(NSUInteger)ub.Offset atIndex:ub.Slot];
                    [enc setFragmentBuffer:mtlBuf offset:(NSUInteger)ub.Offset atIndex:ub.Slot];
                }

                for (const CHEngine::TextureBinding& tb : draw.Textures)
                {
                    if (!tb.Texture.IsValid()) continue;
                    TextureMTLFull* tex = m_Factory.Textures.Get(tb.Texture);
                    if (!tex) continue;
                    if (tex->GetNativeTexture())
                        [enc setFragmentTexture:(id<MTLTexture>)tex->GetNativeTexture() atIndex:tb.Slot];
                    if (tex->GetNativeSampler())
                        [enc setFragmentSamplerState:(id<MTLSamplerState>)tex->GetNativeSampler() atIndex:tb.Slot];
                }

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

        [enc endEncoding];
        [enc release];
        MTLGlobals::g_Encoder = nullptr;
    }

    CHE_CORE_INFO("FrameGraphBackendMTL: all passes done, restoring screen encoder");

    // Restore screen encoder for ImGui
    {
        MTLRenderPassDescriptor* screenRPDesc =
            (MTLRenderPassDescriptor*)MTLGlobals::g_ScreenRPDesc;
        if (screenRPDesc) {
            screenRPDesc.colorAttachments[0].loadAction = MTLLoadActionLoad;

            id<MTLRenderCommandEncoder> screenEnc =
                [cmdBuf renderCommandEncoderWithDescriptor:screenRPDesc];
            if (screenEnc) {
                [screenEnc retain];
                MTLGlobals::g_Encoder = (void*)screenEnc;

                if (MTLGlobals::g_Width > 0 && MTLGlobals::g_Height > 0) {
                    MTLViewport vp = { 0.0, 0.0, (double)MTLGlobals::g_Width,
                                                  (double)MTLGlobals::g_Height, 0.0, 1.0 };
                    [screenEnc setViewport:vp];
                }

                // Update ImGui's render context so it uses the new encoder
                CHEngine::RenderContextInfo ctx;
                ctx.Device               = MTLGlobals::g_Device;
                ctx.CommandBuffer        = (void*)cmdBuf;
                ctx.RenderEncoder        = (void*)screenEnc;
                ctx.RenderPassDescriptor = MTLGlobals::g_ScreenRPDesc;
                if (auto* ui = CHEngine::Application::Get().UI())
                    ui->SetRenderContext(ctx);
            }
        }
    }

    CHE_CORE_INFO("FrameGraphBackendMTL: execute complete");

    } // @autoreleasepool
}

} // namespace CHModules
