#include <cstdint>
#include "ShaderMTL.h"
#include "MetalGlobals.h"

#include <Log/Log.h>
#include <Render/UniformBlocks.h>
#include <SlangBackend/SlangBackend.h>

#import <Metal/Metal.h>

#include <cstring>
#include <cstdlib>
#include <string>
#include <algorithm>

namespace CHModules
{

// ─── Compile .slang → MSL → MTLLibrary ──────────────────────────────────────

bool CHModules::ShaderMTL::CompileSlang(const CHEngine::String& slangSource,
                                        const CHEngine::String& vertEntry,
                                        const CHEngine::String& fragEntry,
                                        const CHEngine::String& sourcePath)
{
    id<MTLDevice> device = (id<MTLDevice>)MTLGlobals::g_Device;
    if (!device) {
        CHE_CORE_ERROR("ShaderMTL: Metal device is null!");
        return false;
    }

    SlangBackend* backend = SlangBackend::GetForApi(CHEngine::ERenderAPI::METAL);
    if (!backend) {
        CHE_CORE_ERROR("ShaderMTL: SlangBackend unavailable for Metal");
        return false;
    }

    CompiledShader compiled = backend->Compile(slangSource, vertEntry, fragEntry, sourcePath);
    if (!compiled.valid) {
        CHE_CORE_ERROR("ShaderMTL: Slang compilation failed:\n{}", compiled.errorLog);
        return false;
    }

    // Slang Metal: getTargetCode() produces ONE MSL with both vertex + fragment.
    // vertexCode and fragmentCode contain the same combined source.
    std::string mslSource(reinterpret_cast<const char*>(compiled.vertexCode.data()),
                          compiled.vertexCode.size());

    NSString* src = [NSString stringWithUTF8String:mslSource.c_str()];
    NSError* error = nil;

    id<MTLLibrary> library = [device newLibraryWithSource:src options:nil error:&error];
    if (!library) {
        CHE_CORE_ERROR("ShaderMTL: MSL compilation failed: {}",
                       error ? [[error localizedDescription] UTF8String] : "unknown");
        return false;
    }

    // Log available function names for diagnostics
    {
        NSArray<NSString*>* fnNames = [library functionNames];
        std::string names;
        for (NSString* n in fnNames)
            names += std::string([n UTF8String]) + " ";
        CHE_CORE_INFO("ShaderMTL: library functions: [{}]", names);
    }

    // Try exact name, then with _0 suffix (Slang sometimes appends _0)
    auto findFunc = [&library](const std::string& name) -> id<MTLFunction> {
        id<MTLFunction> f = [library newFunctionWithName:[NSString stringWithUTF8String:name.c_str()]];
        if (f) return f;
        std::string suffixed = name + "_0";
        return [library newFunctionWithName:[NSString stringWithUTF8String:suffixed.c_str()]];
    };

    id<MTLFunction> vertFunc = findFunc(vertEntry.c_str());
    id<MTLFunction> fragFunc = findFunc(fragEntry.c_str());

    if (!vertFunc) {
        CHE_CORE_ERROR("ShaderMTL: vertex entry '{}' not found in MSL", vertEntry.c_str());
        [library release];
        return false;
    }
    if (!fragFunc) {
        CHE_CORE_ERROR("ShaderMTL: fragment entry '{}' not found in MSL", fragEntry.c_str());
        [vertFunc release];
        [library release];
        return false;
    }

    // Release old resources
    if (m_FragmentFunc) [(id<MTLFunction>)m_FragmentFunc release];
    if (m_VertexFunc)   [(id<MTLFunction>)m_VertexFunc release];
    if (m_Library)      [(id<MTLLibrary>)m_Library release];

    // Clear pipeline cache
    for (auto& [key, pso] : m_PipelineCache)
        [(id<MTLRenderPipelineState>)pso release];
    m_PipelineCache.clear();

    m_Library      = (void*)library;
    m_VertexFunc   = (void*)vertFunc;
    m_FragmentFunc = (void*)fragFunc;

    return true;
}

// ─── Constructor / Destructor ───────────────────────────────────────────────

CHModules::ShaderMTL::ShaderMTL(const CHEngine::String& slangSource,
                                const CHEngine::String& vertEntry,
                                const CHEngine::String& fragEntry,
                                const CHEngine::String& sourcePath)
{
    if (!CompileSlang(slangSource, vertEntry, fragEntry, sourcePath))
        CHE_CORE_ERROR("ShaderMTL: failed to compile '{}'", sourcePath.c_str());
}

CHModules::ShaderMTL::~ShaderMTL()
{
    for (auto& [key, pso] : m_PipelineCache)
        [(id<MTLRenderPipelineState>)pso release];
        
    if (m_VertexFunc)
        [(id<MTLFunction>)m_VertexFunc release];
        
    if (m_FragmentFunc)
        [(id<MTLFunction>)m_FragmentFunc release];
        
    if (m_Library)
        [(id<MTLLibrary>)m_Library release];
}

// ─── Bind / Unbind ──────────────────────────────────────────────────────────

void CHModules::ShaderMTL::Bind() const
{
    // NOP for Metal: Shaders are bound via pipeline states
    // Uniforms flushed in FlushUniforms() when setting up the encoder
}

void CHModules::ShaderMTL::Unbind() const
{
    // NOP for Metal: encoder lifetime handles binding/unbinding
}

// ─── Reload ─────────────────────────────────────────────────────────────────

bool CHModules::ShaderMTL::Reload(const CHEngine::String& slangSource,
                                  const CHEngine::String& vertEntry,
                                  const CHEngine::String& fragEntry,
                                  const CHEngine::String& sourcePath)
{
    return CompileSlang(slangSource, vertEntry, fragEntry, sourcePath);
}

// ─── Pipeline State ─────────────────────────────────────────────────────────

static MTLVertexFormat VertexFormatToMTLFormat(CHEngine::VertexFormat format)
{
    using VF = CHEngine::VertexFormat;
    switch (format)
    {
    case VF::Float:        return MTLVertexFormatFloat;
    case VF::Float2:       return MTLVertexFormatFloat2;
    case VF::Float3:       return MTLVertexFormatFloat3;
    case VF::Float4:       return MTLVertexFormatFloat4;
    case VF::Int:          return MTLVertexFormatInt;
    case VF::Int2:         return MTLVertexFormatInt2;
    case VF::Int3:         return MTLVertexFormatInt3;
    case VF::Int4:         return MTLVertexFormatInt4;
    case VF::UInt:         return MTLVertexFormatUInt;
    case VF::UInt2:        return MTLVertexFormatUInt2;
    case VF::UInt3:        return MTLVertexFormatUInt3;
    case VF::UInt4:        return MTLVertexFormatUInt4;
    case VF::UByte4:       return MTLVertexFormatUChar4;
    case VF::UByte4_Norm:  return MTLVertexFormatUChar4Normalized;
    default:               return MTLVertexFormatFloat3;
    }
}

void* CHModules::ShaderMTL::GetOrCreatePipelineState(const CHEngine::VertexInputLayout& layout,
                                           uint32_t colorPixelFormat,
                                           uint32_t depthPixelFormat,
                                           bool blendEnabled)
{
    CHE_CORE_ASSERT(m_VertexFunc, "ShaderMTL: vertex function not compiled");
    CHE_CORE_ASSERT(m_FragmentFunc, "ShaderMTL: fragment function not compiled");

    id<MTLDevice> device = (id<MTLDevice>)MTLGlobals::g_Device;
    CHE_CORE_ASSERT(device, "ShaderMTL: Metal device is null");

    // Compute cache key
    PipelineCacheKey key = {};
    key.stride      = layout.Strides.empty() ? 0u : layout.Strides[0];  // Use first stride for key
    key.numAttribs  = static_cast<uint32_t>(layout.Attributes.size());
    key.colorFormat = colorPixelFormat;
    key.depthFormat = depthPixelFormat;
    key.blend       = blendEnabled;

    auto it = m_PipelineCache.find(key);
    if (it != m_PipelineCache.end())
        return it->second;

    // Build vertex descriptor
    MTLVertexDescriptor* vertDesc = [[MTLVertexDescriptor alloc] init];
    uint32_t attrIdx = 0;
    for (const auto& attr : layout.Attributes)
    {
        // Map slot index safely - if beyond bounds, use slot 0
        const uint32_t slotIndex = (attr.Slot < static_cast<uint32_t>(layout.Strides.size())) 
                                        ? attr.Slot : 0u;
        
        vertDesc.attributes[attrIdx].format      = VertexFormatToMTLFormat(attr.Format);
        vertDesc.attributes[attrIdx].offset      = attr.Offset;
        vertDesc.attributes[attrIdx].bufferIndex = slotIndex;
        attrIdx++;
    }
    
    // Setup layouts for each vertex buffer slot
    for (size_t slotIdx = 0; slotIdx < layout.Strides.size(); ++slotIdx) {
        vertDesc.layouts[slotIdx].stride       = layout.Strides[slotIdx];
        vertDesc.layouts[slotIdx].stepRate     = 1;
        vertDesc.layouts[slotIdx].stepFunction = MTLVertexStepFunctionPerVertex;
    }

    // Build pipeline descriptor
    MTLRenderPipelineDescriptor* desc = [[MTLRenderPipelineDescriptor alloc] init];
    desc.vertexFunction                  = (__bridge id<MTLFunction>)m_VertexFunc;
    desc.fragmentFunction                = (__bridge id<MTLFunction>)m_FragmentFunc;
    desc.vertexDescriptor                = vertDesc;
    desc.rasterSampleCount               = 1;
    
    // Color attachment
    desc.colorAttachments[0].pixelFormat = (MTLPixelFormat)colorPixelFormat;
    desc.colorAttachments[0].writeMask   = MTLColorWriteMaskAll;
    if (blendEnabled) {
        desc.colorAttachments[0].blendingEnabled             = YES;
        desc.colorAttachments[0].sourceRGBBlendFactor        = MTLBlendFactorSourceAlpha;
        desc.colorAttachments[0].destinationRGBBlendFactor   = MTLBlendFactorOneMinusSourceAlpha;
        desc.colorAttachments[0].sourceAlphaBlendFactor      = MTLBlendFactorOne;
        desc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorZero;
        desc.colorAttachments[0].rgbBlendOperation           = MTLBlendOperationAdd;
        desc.colorAttachments[0].alphaBlendOperation         = MTLBlendOperationAdd;
    } else {
        desc.colorAttachments[0].blendingEnabled = NO;
    }

    // Depth attachment
    desc.depthAttachmentPixelFormat = (MTLPixelFormat)depthPixelFormat;

    NSError* error = nil;
    id<MTLRenderPipelineState> pipeline = [device newRenderPipelineStateWithDescriptor:desc error:&error];
    if (!pipeline) {
        CHE_CORE_ERROR("ShaderMTL: failed to create pipeline state:\n{}", 
                      error.description.UTF8String);
        return nullptr;
    }

    void* pipelinePtr = (__bridge void*)pipeline;
    m_PipelineCache[key] = pipelinePtr;
    return pipelinePtr;
}

void CHModules::ShaderMTL::FlushUniforms(void* encoderPtr) const
{
    id<MTLRenderCommandEncoder> encoder = (id<MTLRenderCommandEncoder>)encoderPtr;

    // buffer(1) = CameraUBO — vertex + fragment
    [encoder setVertexBytes:&m_Camera   length:sizeof(CHEngine::UBOCamera)   atIndex:1];
    [encoder setFragmentBytes:&m_Camera length:sizeof(CHEngine::UBOCamera)   atIndex:1];

    // buffer(2) = ObjectUBO — vertex + fragment
    [encoder setVertexBytes:&m_Object   length:sizeof(CHEngine::UBOObject)   atIndex:2];
    [encoder setFragmentBytes:&m_Object length:sizeof(CHEngine::UBOObject)   atIndex:2];

    // buffer(3) = LightingUBO — fragment only
    [encoder setFragmentBytes:&m_Lighting length:sizeof(CHEngine::UBOLighting) atIndex:3];

    // buffer(4) = MaterialUBO — fragment only
    [encoder setFragmentBytes:&m_Material length:sizeof(CHEngine::UBOMaterial) atIndex:4];
}

void CHModules::ShaderMTL::SetUniformBlock(CHEngine::EUniformBlock block, const void* data, uint32_t size)
{
    switch (block)
    {
    case CHEngine::EUniformBlock::Camera:
        std::memcpy(&m_Camera, data, std::min(size, static_cast<uint32_t>(sizeof(m_Camera))));
        break;
    case CHEngine::EUniformBlock::Object:
        std::memcpy(&m_Object, data, std::min(size, static_cast<uint32_t>(sizeof(m_Object))));
        break;
    case CHEngine::EUniformBlock::Lighting:
        std::memcpy(&m_Lighting, data, std::min(size, static_cast<uint32_t>(sizeof(m_Lighting))));
        break;
    case CHEngine::EUniformBlock::Material:
        std::memcpy(&m_Material, data, std::min(size, static_cast<uint32_t>(sizeof(m_Material))));
        break;
    case CHEngine::EUniformBlock::COUNT:
        // Ignore COUNT value
        break;
    }
}

void CHModules::ShaderMTL::SetInt(const CHEngine::String& /*name*/, int /*value*/)
{
    // Metal привязывает текстуры напрямую через [[texture(N)]],
    // sampler slot не нужен.
}
} // namespace CHModules
