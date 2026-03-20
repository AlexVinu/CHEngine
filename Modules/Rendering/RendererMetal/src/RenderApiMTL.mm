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

RenderApiMTL::~RenderApiMTL()
{
    if (m_DepthStencilState)
        [(id<MTLDepthStencilState>)m_DepthStencilState release];
}

void RenderApiMTL::SetClearColor(float r, float g, float b, float a)
{
    // Передаём в MetalContext — он хранит clear color и применяет его
    // в BeginFrame при создании render pass descriptor
    if (MTLGlobals::g_ContextPtr)
        static_cast<MetalContext*>(MTLGlobals::g_ContextPtr)->SetClearColor(r, g, b, a);
}

void RenderApiMTL::Clear()
{
    // Clear выполняется через MTLLoadActionClear в render pass descriptor
}

// ─── Кеширование depth stencil state ──────────────────────────────────────

void RenderApiMTL::UpdateDepthStencilState()
{
    if (!m_DepthStateDirty) return;
    m_DepthStateDirty = false;

    id<MTLDevice> device = (id<MTLDevice>)MTLGlobals::g_Device;
    if (!device || MTLGlobals::g_DepthPixelFormat == 0) return;

    if (m_DepthStencilState)
        [(id<MTLDepthStencilState>)m_DepthStencilState release];

    MTLDepthStencilDescriptor* depthDesc = [[MTLDepthStencilDescriptor alloc] init];
    depthDesc.depthCompareFunction = MTLCompareFunctionLess;
    depthDesc.depthWriteEnabled = MTLGlobals::g_DepthWriteEnabled;
    id<MTLDepthStencilState> state = [device newDepthStencilStateWithDescriptor:depthDesc];
    [depthDesc release];

    m_DepthStencilState = (void*)state;
}

// ─── Draw ────────────────────────────────────────────────────────────────

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

    // Depth stencil (кешируется, пересоздаётся только при изменении)
    UpdateDepthStencilState();
    if (m_DepthStencilState)
        [encoder setDepthStencilState:(id<MTLDepthStencilState>)m_DepthStencilState];

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

    // Depth stencil — используем тот же кеш
    UpdateDepthStencilState();
    if (m_DepthStencilState)
        [encoder setDepthStencilState:(id<MTLDepthStencilState>)m_DepthStencilState];

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
    if (MTLGlobals::g_DepthWriteEnabled != enable) {
        MTLGlobals::g_DepthWriteEnabled = enable;
        m_DepthStateDirty = true; // пересоздать depth stencil state
    }
}

} // namespace CHModules
