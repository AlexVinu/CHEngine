#include <cstdint>
#include "RenderApiMTL.h"
#include "MetalGlobals.h"
#include "MetalContext.h"
#include "ShaderMTL.h"
#include "BufferMTL.h"
#include "VertexArrayMTL.h"

#include <Log/Log.h>

#import <Metal/Metal.h>

namespace CHModules
{

RenderApiMTL::RenderApiMTL()
{
    MTLGlobals::g_DepthWriteEnabled = true;
    MTLGlobals::g_BlendEnabled = false;
}

void RenderApiMTL::SetClearColor(float /*r*/, float /*g*/, float /*b*/, float /*a*/)
{
    // Clear color is set via MetalContext's render pass descriptor
}

void RenderApiMTL::Clear()
{
    // Clear is handled by MTLLoadActionClear in the render pass descriptor
}

void RenderApiMTL::DrawIndexed(const CHEngine::IVertexArray* vertexArray)
{
    if (!vertexArray) return;

    id<MTLRenderCommandEncoder> encoder = (id<MTLRenderCommandEncoder>)MTLGlobals::g_Encoder;
    if (!encoder) return;

    ShaderMTL* shader = (ShaderMTL*)MTLGlobals::g_BoundShader;
    if (!shader) {
        CHE_CORE_WARN("RenderApiMTL::DrawIndexed — no shader bound");
        return;
    }

    // Get vertex buffer layout for pipeline state creation
    const auto& vertexBuffers = vertexArray->GetVertexBuffers();
    if (vertexBuffers.empty()) return;

    const auto& layout = vertexBuffers[0]->GetLayout();
    if (layout.GetElements().empty()) return;

    // Get or create pipeline state
    void* pso = shader->GetOrCreatePipelineState(
        layout,
        MTLGlobals::g_ColorPixelFormat,
        MTLGlobals::g_DepthPixelFormat,
        MTLGlobals::g_BlendEnabled
    );
    if (!pso) return;

    [encoder setRenderPipelineState:(id<MTLRenderPipelineState>)pso];

    // Set depth stencil state
    id<MTLDevice> device = (id<MTLDevice>)MTLGlobals::g_Device;
    if (device && MTLGlobals::g_DepthPixelFormat != 0) {
        MTLDepthStencilDescriptor* depthDesc = [[MTLDepthStencilDescriptor alloc] init];
        depthDesc.depthCompareFunction = MTLCompareFunctionLess;
        depthDesc.depthWriteEnabled = MTLGlobals::g_DepthWriteEnabled;
        id<MTLDepthStencilState> depthState = [device newDepthStencilStateWithDescriptor:depthDesc];
        [encoder setDepthStencilState:depthState];
        [depthDesc release];
        [depthState release];
    }

    // Flush uniforms
    shader->FlushUniforms((void*)encoder);

    // Bind vertex buffers
    for (const auto& vb : vertexBuffers) {
        auto* mtlVB = static_cast<const VertexBufferMTL*>(vb.get());
        if (mtlVB && mtlVB->GetNativeBuffer()) {
            [encoder setVertexBuffer:(id<MTLBuffer>)mtlVB->GetNativeBuffer()
                              offset:0
                             atIndex:0];
        }
    }

    // Draw with index buffer
    const auto& ib = vertexArray->GetIndexBuffer();
    if (!ib) return;

    auto* mtlIB = static_cast<const IndexBufferMTL*>(ib.get());
    if (!mtlIB || !mtlIB->GetNativeBuffer()) return;

    [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                        indexCount:ib->GetCount()
                         indexType:MTLIndexTypeUInt32
                       indexBuffer:(id<MTLBuffer>)mtlIB->GetNativeBuffer()
                 indexBufferOffset:0];
}

void RenderApiMTL::DrawLines(const CHEngine::IVertexArray* vertexArray)
{
    if (!vertexArray) return;

    id<MTLRenderCommandEncoder> encoder = (id<MTLRenderCommandEncoder>)MTLGlobals::g_Encoder;
    if (!encoder) return;

    ShaderMTL* shader = (ShaderMTL*)MTLGlobals::g_BoundShader;
    if (!shader) return;

    const auto& vertexBuffers = vertexArray->GetVertexBuffers();
    if (vertexBuffers.empty()) return;

    const auto& layout = vertexBuffers[0]->GetLayout();
    if (layout.GetElements().empty()) return;

    void* pso = shader->GetOrCreatePipelineState(
        layout,
        MTLGlobals::g_ColorPixelFormat,
        MTLGlobals::g_DepthPixelFormat,
        MTLGlobals::g_BlendEnabled
    );
    if (!pso) return;

    [encoder setRenderPipelineState:(id<MTLRenderPipelineState>)pso];
    shader->FlushUniforms((void*)encoder);

    for (const auto& vb : vertexBuffers) {
        auto* mtlVB = static_cast<const VertexBufferMTL*>(vb.get());
        if (mtlVB && mtlVB->GetNativeBuffer()) {
            [encoder setVertexBuffer:(id<MTLBuffer>)mtlVB->GetNativeBuffer()
                              offset:0
                             atIndex:0];
        }
    }

    const auto& ib = vertexArray->GetIndexBuffer();
    if (!ib) return;

    auto* mtlIB = static_cast<const IndexBufferMTL*>(ib.get());
    if (!mtlIB || !mtlIB->GetNativeBuffer()) return;

    [encoder drawIndexedPrimitives:MTLPrimitiveTypeLine
                        indexCount:ib->GetCount()
                         indexType:MTLIndexTypeUInt32
                       indexBuffer:(id<MTLBuffer>)mtlIB->GetNativeBuffer()
                 indexBufferOffset:0];
}

void RenderApiMTL::SetViewport(uint32_t width, uint32_t height)
{
    // Forward to MetalContext to update CAMetalLayer.drawableSize and depth texture
    if (MTLGlobals::g_ContextPtr)
        static_cast<MetalContext*>(MTLGlobals::g_ContextPtr)->SetViewport(width, height);
}

void RenderApiMTL::SetBlend(bool enable)
{
    MTLGlobals::g_BlendEnabled = enable;
}

void RenderApiMTL::SetDepthWrite(bool enable)
{
    MTLGlobals::g_DepthWriteEnabled = enable;
}

} // namespace CHModules
