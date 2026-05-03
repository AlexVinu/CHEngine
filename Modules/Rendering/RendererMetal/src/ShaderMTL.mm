#include <cstdint>
#include "ShaderMTL.h"
#include "MetalGlobals.h"

#include <Log/Log.h>
#include <Render/IBuffer.h>
#include <SlangBackend/SlangBackend.h>

#import <Metal/Metal.h>

#include <cstring>
#include <cstdlib>
#include <string>
#include <algorithm>

namespace CHModules
{

// ─── Compile .slang → MSL → MTLLibrary ──────────────────────────────────────

bool ShaderMTL::CompileSlang(const CHEngine::String& slangSource,
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

    // Slang выдаёт одну MSL-строку на программу — обе записи содержат тот же текст.
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

    // Slang appends _0 to entry point names in MSL output — try exact name first,
    // then fall back to the _0-suffixed variant.
    auto findMTLFunc = [&library](const char* name) -> id<MTLFunction> {
        NSString* exact = [NSString stringWithUTF8String:name];
        id<MTLFunction> f = [library newFunctionWithName:exact];
        if (f) return f;
        NSString* suffixed = [NSString stringWithFormat:@"%s_0", name];
        return [library newFunctionWithName:suffixed];
    };

    id<MTLFunction> vertFunc = findMTLFunc(vertEntry.c_str());
    id<MTLFunction> fragFunc = findMTLFunc(fragEntry.c_str());

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

ShaderMTL::ShaderMTL(const CHEngine::String& slangSource,
                     const CHEngine::String& vertEntry,
                     const CHEngine::String& fragEntry,
                     const CHEngine::String& sourcePath)
{
    if (!CompileSlang(slangSource, vertEntry, fragEntry, sourcePath)) {
        CHE_CORE_ERROR("ShaderMTL: failed to compile shader");
    }
}

ShaderMTL::~ShaderMTL()
{
    for (auto& [key, pso] : m_PipelineCache)
        [(id<MTLRenderPipelineState>)pso release];
    m_PipelineCache.clear();

    if (m_FragmentFunc) [(id<MTLFunction>)m_FragmentFunc release];
    if (m_VertexFunc)   [(id<MTLFunction>)m_VertexFunc release];
    if (m_Library)      [(id<MTLLibrary>)m_Library release];
}

// ─── Bind / Unbind ──────────────────────────────────────────────────────────

void ShaderMTL::Bind() const
{
    MTLGlobals::g_BoundShader = const_cast<ShaderMTL*>(this);
}

void ShaderMTL::Unbind() const
{
    if (MTLGlobals::g_BoundShader == this)
        MTLGlobals::g_BoundShader = nullptr;
}

// ─── Reload ─────────────────────────────────────────────────────────────────

bool ShaderMTL::Reload(const CHEngine::String& slangSource,
                       const CHEngine::String& vertEntry,
                       const CHEngine::String& fragEntry,
                       const CHEngine::String& sourcePath)
{
    return CompileSlang(slangSource, vertEntry, fragEntry, sourcePath);
}

// ─── Pipeline State ─────────────────────────────────────────────────────────

static MTLVertexFormat ShaderDataTypeToMTLFormat(CHEngine::ShaderDataType type)
{
    switch (type)
    {
    case CHEngine::ShaderDataType::Float:  return MTLVertexFormatFloat;
    case CHEngine::ShaderDataType::Float2: return MTLVertexFormatFloat2;
    case CHEngine::ShaderDataType::Float3: return MTLVertexFormatFloat3;
    case CHEngine::ShaderDataType::Float4: return MTLVertexFormatFloat4;
    case CHEngine::ShaderDataType::Int:    return MTLVertexFormatInt;
    case CHEngine::ShaderDataType::Int2:   return MTLVertexFormatInt2;
    case CHEngine::ShaderDataType::Int3:   return MTLVertexFormatInt3;
    case CHEngine::ShaderDataType::Int4:   return MTLVertexFormatInt4;
    default: return MTLVertexFormatFloat3;
    }
}

void* ShaderMTL::GetOrCreatePipelineState(const CHEngine::BufferLayout& layout,
                                           uint32_t colorPixelFormat,
                                           uint32_t depthPixelFormat,
                                           bool blendEnabled)
{
    PipelineCacheKey key;
    key.stride      = layout.GetStride();
    key.numAttribs  = (uint32_t)layout.GetElements().size();
    key.colorFormat  = colorPixelFormat;
    key.depthFormat  = depthPixelFormat;
    key.blend       = blendEnabled;

    auto it = m_PipelineCache.find(key);
    if (it != m_PipelineCache.end())
        return it->second;

    // Build vertex descriptor
    MTLVertexDescriptor* vertDesc = [[MTLVertexDescriptor alloc] init];
    uint32_t attrIdx = 0;
    for (const auto& elem : layout)
    {
        vertDesc.attributes[attrIdx].format      = ShaderDataTypeToMTLFormat(elem.Type);
        vertDesc.attributes[attrIdx].offset      = elem.Offset;
        vertDesc.attributes[attrIdx].bufferIndex = 0;
        attrIdx++;
    }
    vertDesc.layouts[0].stride       = layout.GetStride();
    vertDesc.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
    vertDesc.layouts[0].stepRate     = 1;

    // Create pipeline descriptor
    MTLRenderPipelineDescriptor* desc = [[MTLRenderPipelineDescriptor alloc] init];
    desc.vertexFunction   = (id<MTLFunction>)m_VertexFunc;
    desc.fragmentFunction = (id<MTLFunction>)m_FragmentFunc;
    desc.vertexDescriptor = vertDesc;
    desc.colorAttachments[0].pixelFormat = (MTLPixelFormat)colorPixelFormat;

    if (blendEnabled) {
        desc.colorAttachments[0].blendingEnabled             = YES;
        desc.colorAttachments[0].sourceRGBBlendFactor        = MTLBlendFactorSourceAlpha;
        desc.colorAttachments[0].destinationRGBBlendFactor   = MTLBlendFactorOneMinusSourceAlpha;
        desc.colorAttachments[0].rgbBlendOperation           = MTLBlendOperationAdd;
        desc.colorAttachments[0].sourceAlphaBlendFactor      = MTLBlendFactorOne;
        desc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        desc.colorAttachments[0].alphaBlendOperation         = MTLBlendOperationAdd;
    }

    if (depthPixelFormat != 0)
        desc.depthAttachmentPixelFormat = (MTLPixelFormat)depthPixelFormat;

    NSError* error = nil;
    id<MTLDevice> device = (id<MTLDevice>)MTLGlobals::g_Device;
    id<MTLRenderPipelineState> pso = [device newRenderPipelineStateWithDescriptor:desc error:&error];

    [desc release];
    [vertDesc release];

    if (!pso) {
        CHE_CORE_ERROR("ShaderMTL: Pipeline state creation failed: {}",
                       error ? [[error localizedDescription] UTF8String] : "unknown");
        return nullptr;
    }

    m_PipelineCache[key] = (void*)pso;
    return (void*)pso;
}

// ─── Flush uniforms to encoder ──────────────────────────────────────────────

void ShaderMTL::FlushUniforms(void* encoderPtr) const
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

// ─── Uniform Block setter ───────────────────────────────────────────────────

void ShaderMTL::SetUniformBlock(CHEngine::EUniformBlock block, const void* data, uint32_t size)
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
    }
}

// ─── SetInt — no-op для Metal (текстуры через [[texture(N)]]) ──────────────

void ShaderMTL::SetInt(const CHEngine::String& /*name*/, int /*value*/)
{
    // Metal привязывает текстуры напрямую через [[texture(N)]],
    // sampler slot не нужен.
}

} // namespace CHModules
