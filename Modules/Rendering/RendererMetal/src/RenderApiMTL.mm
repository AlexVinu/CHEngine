#include <cstdint>
#include "RenderApiMTL.h"
#include "MetalGlobals.h"
#include "ShaderMTL.h"
#include "BufferMTL.h"
#include "VertexArrayMTL.h"

#include <Log/Log.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>

namespace CHModules
{

RenderApiMTL::RenderApiMTL()
{
    MTLGlobals::g_DepthWriteEnabled = true;
    MTLGlobals::g_BlendEnabled = false;
    MTLGlobals::g_DepthStencilStateDirty = true;
}

RenderApiMTL::~RenderApiMTL()
{
    if (m_DepthStencilState)
        [(id<MTLDepthStencilState>)m_DepthStencilState release];
}

void RenderApiMTL::Init(const CHEngine::RendererInitInfo& init_info)
{
    void* nsWindow = init_info.Metal.NSWindow;
    if (!nsWindow) {
        CHE_CORE_CRITICAL("RenderApiMTL::Init — NSWindow is NULL!");
        return;
    }

    NSWindow* window = (NSWindow*)nsWindow;
    NSRect frame = [[window contentView] frame];
    uint32_t w = static_cast<uint32_t>(frame.size.width);
    uint32_t h = static_cast<uint32_t>(frame.size.height);

    if (!m_Context.Init(nsWindow, w, h)) {
        CHE_CORE_CRITICAL("RenderApiMTL::Init — MetalContext init failed!");
    }
}

void RenderApiMTL::Shutdown()
{
    m_Context.Shutdown();
}

bool RenderApiMTL::BeginFrame()
{
    return m_Context.BeginFrame();
}

void RenderApiMTL::EndFrame()
{
    m_Context.EndFrame();
}

CHEngine::RenderContextInfo RenderApiMTL::GetRenderContext() const
{
    CHEngine::RenderContextInfo ctxInfo;
    ctxInfo.Device               = m_Context.GetDevice();
    ctxInfo.CommandBuffer        = MTLGlobals::g_CommandBuffer;
    ctxInfo.RenderEncoder        = MTLGlobals::g_Encoder;
    ctxInfo.RenderPassDescriptor = m_Context.GetRenderPassDescriptor();
    return ctxInfo;
}

void RenderApiMTL::SetClearColor(float r, float g, float b, float a)
{
    if (MTLGlobals::g_ContextPtr)
        static_cast<MetalContext*>(MTLGlobals::g_ContextPtr)->SetClearColor(r, g, b, a);
}

void RenderApiMTL::Clear()
{
}

void RenderApiMTL::SetViewport(uint32_t width, uint32_t height)
{
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
        MTLGlobals::g_DepthStencilStateDirty = true;
    }
}

void RenderApiMTL::UpdateDepthStencilState()
{
    if (!MTLGlobals::g_DepthStencilStateDirty) return;
    MTLGlobals::g_DepthStencilStateDirty = false;

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

void RenderApiMTL::DrawIndexed(class VertexArrayMTL* vertexArray)
{
    if (!vertexArray) return;

    id<MTLRenderCommandEncoder> encoder = (id<MTLRenderCommandEncoder>)MTLGlobals::g_Encoder;
    if (!encoder) return;

    ShaderMTL* shader = (ShaderMTL*)MTLGlobals::g_BoundShader;
    if (!shader) {
        CHE_CORE_WARN("RenderApiMTL::DrawIndexed — no shader bound");
        return;
    }

    const auto& vertexBuffers = vertexArray->GetVertexBuffers();
    if (vertexBuffers.empty()) return;

    const auto& layout = vertexBuffers[0]->GetLayout();
    if (layout.Attributes.empty()) return;

    void* pso = shader->GetOrCreatePipelineState(
        layout,
        MTLGlobals::g_ColorPixelFormat,
        MTLGlobals::g_DepthPixelFormat,
        MTLGlobals::g_BlendEnabled
    );
    if (!pso) return;

    [encoder setRenderPipelineState:(id<MTLRenderPipelineState>)pso];

    UpdateDepthStencilState();
    if (m_DepthStencilState)
        [encoder setDepthStencilState:(id<MTLDepthStencilState>)m_DepthStencilState];

    shader->FlushUniforms((void*)encoder);

    for (const auto& vb : vertexBuffers) {
        auto* mtlVB = static_cast<const VertexBufferMTL*>(vb);
        if (mtlVB && mtlVB->GetNativeBuffer()) {
            [encoder setVertexBuffer:(id<MTLBuffer>)mtlVB->GetNativeBuffer()
                              offset:0
                             atIndex:0];
        }
    }

    class IndexBufferMTL* ib = vertexArray->GetIndexBuffer();
    if (!ib) return;

    auto* mtlIB = static_cast<const IndexBufferMTL*>(ib);
    if (!mtlIB || !mtlIB->GetNativeBuffer()) return;

    [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                        indexCount:mtlIB->GetCount()
                         indexType:MTLIndexTypeUInt32
                       indexBuffer:(id<MTLBuffer>)mtlIB->GetNativeBuffer()
                 indexBufferOffset:0];
}

void RenderApiMTL::DrawLines(class VertexArrayMTL* vertexArray)
{
    if (!vertexArray) return;

    id<MTLRenderCommandEncoder> encoder = (id<MTLRenderCommandEncoder>)MTLGlobals::g_Encoder;
    if (!encoder) return;

    ShaderMTL* shader = (ShaderMTL*)MTLGlobals::g_BoundShader;
    if (!shader) return;

    const auto& vertexBuffers = vertexArray->GetVertexBuffers();
    if (vertexBuffers.empty()) return;

    const auto& layout = vertexBuffers[0]->GetLayout();
    if (layout.Attributes.empty()) return;

    void* pso = shader->GetOrCreatePipelineState(
        layout,
        MTLGlobals::g_ColorPixelFormat,
        MTLGlobals::g_DepthPixelFormat,
        MTLGlobals::g_BlendEnabled
    );
    if (!pso) return;

    [encoder setRenderPipelineState:(id<MTLRenderPipelineState>)pso];

    UpdateDepthStencilState();
    if (m_DepthStencilState)
        [encoder setDepthStencilState:(id<MTLDepthStencilState>)m_DepthStencilState];

    shader->FlushUniforms((void*)encoder);

    for (const auto& vb : vertexBuffers) {
        auto* mtlVB = static_cast<const VertexBufferMTL*>(vb);
        if (mtlVB && mtlVB->GetNativeBuffer()) {
            [encoder setVertexBuffer:(id<MTLBuffer>)mtlVB->GetNativeBuffer()
                              offset:0
                             atIndex:0];
        }
    }

    class IndexBufferMTL* ib = vertexArray->GetIndexBuffer();
    if (!ib) return;

    auto* mtlIB = static_cast<const IndexBufferMTL*>(ib);
    if (!mtlIB || !mtlIB->GetNativeBuffer()) return;

    [encoder drawIndexedPrimitives:MTLPrimitiveTypeLine
                        indexCount:mtlIB->GetCount()
                         indexType:MTLIndexTypeUInt32
                       indexBuffer:(id<MTLBuffer>)mtlIB->GetNativeBuffer()
                 indexBufferOffset:0];
}

} // namespace CHModules
