#include "PipelineMTL.h"
#include "RenderFactoryMTL.h"
#include "ShaderMTL.h"
#include "MetalGlobals.h"

#import <Metal/Metal.h>
#include <Log/Log.h>

namespace CHModules {

// ─── Format helpers ───────────────────────────────────────────────────────────
uint32_t PipelineMTL::ToMTLPixelFormat(CHEngine::TextureFormat fmt)
{
    using F = CHEngine::TextureFormat;
    switch (fmt) {
        case F::RGBA8_UNORM:       return MTLPixelFormatRGBA8Unorm;
        case F::RGBA8_UNORM_SRGB:  return MTLPixelFormatRGBA8Unorm_sRGB;
        case F::RGBA16_FLOAT:      return MTLPixelFormatRGBA16Float;
        case F::RGBA32_FLOAT:      return MTLPixelFormatRGBA32Float;
        case F::R8_UNORM:          return MTLPixelFormatR8Unorm;
        case F::RG8_UNORM:         return MTLPixelFormatRG8Unorm;
        case F::R16_FLOAT:         return MTLPixelFormatR16Float;
        case F::RG16_FLOAT:        return MTLPixelFormatRG16Float;
        case F::R32_FLOAT:         return MTLPixelFormatR32Float;
        case F::D32_FLOAT:         return MTLPixelFormatDepth32Float;
        case F::D24_UNORM_S8_UINT: return MTLPixelFormatDepth32Float;
        case F::D32_FLOAT_S8_UINT: return MTLPixelFormatDepth32Float_Stencil8;
        default:                   return MTLPixelFormatRGBA8Unorm;
    }
}

uint32_t PipelineMTL::ToMTLCompareFunc(CHEngine::CompareOp op)
{
    switch (op) {
        case CHEngine::CompareOp::Never:        return MTLCompareFunctionNever;
        case CHEngine::CompareOp::Less:         return MTLCompareFunctionLess;
        case CHEngine::CompareOp::Equal:        return MTLCompareFunctionEqual;
        case CHEngine::CompareOp::LessEqual:    return MTLCompareFunctionLessEqual;
        case CHEngine::CompareOp::Greater:      return MTLCompareFunctionGreater;
        case CHEngine::CompareOp::NotEqual:     return MTLCompareFunctionNotEqual;
        case CHEngine::CompareOp::GreaterEqual: return MTLCompareFunctionGreaterEqual;
        case CHEngine::CompareOp::Always:       return MTLCompareFunctionAlways;
        default:                                return MTLCompareFunctionLess;
    }
}

uint32_t PipelineMTL::ToMTLBlendFactor(CHEngine::BlendFactor f)
{
    switch (f) {
        case CHEngine::BlendFactor::Zero:             return MTLBlendFactorZero;
        case CHEngine::BlendFactor::One:              return MTLBlendFactorOne;
        case CHEngine::BlendFactor::SrcAlpha:         return MTLBlendFactorSourceAlpha;
        case CHEngine::BlendFactor::OneMinusSrcAlpha: return MTLBlendFactorOneMinusSourceAlpha;
        case CHEngine::BlendFactor::DstAlpha:         return MTLBlendFactorDestinationAlpha;
        case CHEngine::BlendFactor::OneMinusDstAlpha: return MTLBlendFactorOneMinusDestinationAlpha;
        default:                                      return MTLBlendFactorOne;
    }
}

uint32_t PipelineMTL::ToMTLBlendOp(CHEngine::BlendOp op)
{
    switch (op) {
        case CHEngine::BlendOp::Add:             return MTLBlendOperationAdd;
        case CHEngine::BlendOp::Subtract:        return MTLBlendOperationSubtract;
        case CHEngine::BlendOp::ReverseSubtract: return MTLBlendOperationReverseSubtract;
        case CHEngine::BlendOp::Min:             return MTLBlendOperationMin;
        case CHEngine::BlendOp::Max:             return MTLBlendOperationMax;
        default:                                 return MTLBlendOperationAdd;
    }
}

// ─── Constructor ──────────────────────────────────────────────────────────────
PipelineMTL::PipelineMTL(const CHEngine::PipelineDesc& desc,
                         RenderFactoryMTL& factory)
    : m_Desc(desc)
{
    id<MTLDevice> device = (id<MTLDevice>)MTLGlobals::g_Device;
    if (!device) { CHE_CORE_ERROR("PipelineMTL: device is null"); return; }

    // ── Lookup shader functions ───────────────────────────────────────────────
    ShaderMTL* shader = factory.Shaders.Get(desc.Shader);
    if (!shader) {
        CHE_CORE_ERROR("PipelineMTL: shader not found");
        return;
    }

    // ── Render pipeline descriptor ────────────────────────────────────────────
    MTLRenderPipelineDescriptor* psoDesc = [[MTLRenderPipelineDescriptor alloc] init];
    psoDesc.vertexFunction   = (id<MTLFunction>)shader->GetVertexFunc();
    psoDesc.fragmentFunction = (id<MTLFunction>)shader->GetFragmentFunc();

    // Color attachments
    for (size_t i = 0; i < desc.ColorFormats.size(); ++i) {
        MTLPixelFormat colorFmt = (MTLPixelFormat)ToMTLPixelFormat(desc.ColorFormats[i]);
        psoDesc.colorAttachments[i].pixelFormat = colorFmt;

        if (desc.Blend.Enable) {
            psoDesc.colorAttachments[i].blendingEnabled             = YES;
            psoDesc.colorAttachments[i].sourceRGBBlendFactor        = (MTLBlendFactor)ToMTLBlendFactor(desc.Blend.SrcColor);
            psoDesc.colorAttachments[i].destinationRGBBlendFactor   = (MTLBlendFactor)ToMTLBlendFactor(desc.Blend.DstColor);
            psoDesc.colorAttachments[i].rgbBlendOperation           = (MTLBlendOperation)ToMTLBlendOp(desc.Blend.ColorOp);
            psoDesc.colorAttachments[i].sourceAlphaBlendFactor      = (MTLBlendFactor)ToMTLBlendFactor(desc.Blend.SrcAlpha);
            psoDesc.colorAttachments[i].destinationAlphaBlendFactor = (MTLBlendFactor)ToMTLBlendFactor(desc.Blend.DstAlpha);
            psoDesc.colorAttachments[i].alphaBlendOperation         = (MTLBlendOperation)ToMTLBlendOp(desc.Blend.AlphaOp);
        }
    }

    // Depth format
    const bool hasDepth = (desc.DepthFormat == CHEngine::TextureFormat::D32_FLOAT ||
                           desc.DepthFormat == CHEngine::TextureFormat::D24_UNORM_S8_UINT ||
                           desc.DepthFormat == CHEngine::TextureFormat::D32_FLOAT_S8_UINT);
    if (hasDepth) {
        MTLPixelFormat depthFmt = (MTLPixelFormat)ToMTLPixelFormat(desc.DepthFormat);
        if (depthFmt == MTLPixelFormatDepth32Float_Stencil8)
            psoDesc.stencilAttachmentPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
        psoDesc.depthAttachmentPixelFormat = depthFmt;
    }

    // ── Vertex descriptor from VertexInputLayout ──────────────────────────────
    if (!desc.VertexLayout.Attributes.empty()) {
        MTLVertexDescriptor* vd = [[MTLVertexDescriptor alloc] init];
        for (size_t i = 0; i < desc.VertexLayout.Attributes.size(); ++i) {
            const auto& attr = desc.VertexLayout.Attributes[i];
            // Map VertexFormat to MTLVertexFormat
            MTLVertexFormat mtlFmt = MTLVertexFormatFloat3;
            switch (attr.Format) {
                case CHEngine::VertexFormat::Float:  mtlFmt = MTLVertexFormatFloat;  break;
                case CHEngine::VertexFormat::Float2: mtlFmt = MTLVertexFormatFloat2; break;
                case CHEngine::VertexFormat::Float3: mtlFmt = MTLVertexFormatFloat3; break;
                case CHEngine::VertexFormat::Float4: mtlFmt = MTLVertexFormatFloat4; break;
                default: break;
            }
            vd.attributes[i].format      = mtlFmt;
            vd.attributes[i].offset      = attr.Offset;
            vd.attributes[i].bufferIndex = 10; // vertex data at slot 10 (above UBO slots 0-3)
        }
        // Set stride for slot 10
        if (!desc.VertexLayout.Strides.empty()) {
            vd.layouts[10].stride       = desc.VertexLayout.Strides[0];
            vd.layouts[10].stepFunction = MTLVertexStepFunctionPerVertex;
        }
        psoDesc.vertexDescriptor = vd;
        [vd release];
    }

    // ── Create PSO ────────────────────────────────────────────────────────────
    NSError* err = nil;
    id<MTLRenderPipelineState> pso = [device newRenderPipelineStateWithDescriptor:psoDesc error:&err];
    [psoDesc release];

    if (!pso) {
        CHE_CORE_ERROR("PipelineMTL: PSO creation failed: {}",
                       err ? [[err localizedDescription] UTF8String] : "unknown");
        return;
    }
    m_PSO = (void*)pso;

    // ── Depth stencil state ───────────────────────────────────────────────────
    if (desc.Depth.Test || desc.Depth.Write) {
        MTLDepthStencilDescriptor* ds = [[MTLDepthStencilDescriptor alloc] init];
        ds.depthCompareFunction = desc.Depth.Test
            ? (MTLCompareFunction)ToMTLCompareFunc(desc.Depth.Compare)
            : MTLCompareFunctionAlways;
        ds.depthWriteEnabled = desc.Depth.Write ? YES : NO;
        m_DepthState = (void*)[device newDepthStencilStateWithDescriptor:ds];
        [ds release];
    }
}

PipelineMTL::~PipelineMTL()
{
    if (m_PSO)        { [(id<MTLRenderPipelineState>)m_PSO release];        m_PSO = nullptr; }
    if (m_DepthState) { [(id<MTLDepthStencilState>)m_DepthState release];   m_DepthState = nullptr; }
}

} // namespace CHModules
