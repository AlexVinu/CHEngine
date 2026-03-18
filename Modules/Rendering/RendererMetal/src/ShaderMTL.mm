#include <cstdint>
#include "ShaderMTL.h"
#include "MetalGlobals.h"

#include <Log/Log.h>
#include <Render/IBuffer.h>

#import <Metal/Metal.h>

#include <cstring>
#include <cstdlib>
#include <string>

namespace CHModules
{

// ─── Compile MSL source ─────────────────────────────────────────────────────

bool ShaderMTL::CompileSource(const CHEngine::String& source)
{
    id<MTLDevice> device = (id<MTLDevice>)MTLGlobals::g_Device;
    if (!device) {
        CHE_CORE_ERROR("ShaderMTL: Metal device is null!");
        return false;
    }

    NSString* src = [NSString stringWithUTF8String:source.c_str()];
    NSError* error = nil;

    id<MTLLibrary> library = [device newLibraryWithSource:src options:nil error:&error];
    if (!library) {
        CHE_CORE_ERROR("ShaderMTL: MSL compilation failed: {}",
                       error ? [[error localizedDescription] UTF8String] : "unknown");
        return false;
    }

    id<MTLFunction> vertFunc = [library newFunctionWithName:@"vertexMain"];
    id<MTLFunction> fragFunc = [library newFunctionWithName:@"fragmentMain"];

    if (!vertFunc) {
        CHE_CORE_ERROR("ShaderMTL: 'vertexMain' function not found in MSL source");
        [library release];
        return false;
    }
    if (!fragFunc) {
        CHE_CORE_ERROR("ShaderMTL: 'fragmentMain' function not found in MSL source");
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

ShaderMTL::ShaderMTL(const CHEngine::String& vertexSrc, const CHEngine::String& /*fragmentSrc*/)
{
    // For Metal, vertexSrc contains the full .metal source
    if (!CompileSource(vertexSrc)) {
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

bool ShaderMTL::Reload(const CHEngine::String& vertexSrc, const CHEngine::String& /*fragmentSrc*/)
{
    return CompileSource(vertexSrc);
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
    // Vertex stage: uniforms at buffer(1)
    [encoder setVertexBytes:&m_VertexUniforms   length:sizeof(MTLVertexUniforms)   atIndex:1];
    // Fragment stage: fragment uniforms at buffer(0), vertex uniforms at buffer(1)
    // (grid.metal reads u_InvViewProj from fragment [[buffer(1)]])
    [encoder setFragmentBytes:&m_FragmentUniforms length:sizeof(MTLFragmentUniforms) atIndex:0];
    [encoder setFragmentBytes:&m_VertexUniforms   length:sizeof(MTLVertexUniforms)   atIndex:1];
}

// ─── Uniform setters ────────────────────────────────────────────────────────

int ShaderMTL::ParseArrayIndex(const char* name, const char* prefix) const
{
    size_t prefixLen = strlen(prefix);
    if (strncmp(name, prefix, prefixLen) != 0) return -1;
    if (name[prefixLen] != '[') return -1;
    int idx = atoi(name + prefixLen + 1);
    if (idx < 0 || idx >= 8) return -1;
    return idx;
}

void ShaderMTL::SetInt(const CHEngine::String& name, int value)
{
    const char* n = name.c_str();

    if (strcmp(n, "u_NumLights") == 0)      { m_FragmentUniforms.NumLights = value; return; }
    if (strcmp(n, "u_UseTexture") == 0)      { m_FragmentUniforms.UseTexture = value; return; }
    if (strcmp(n, "u_UseSpecularMap") == 0)  { m_FragmentUniforms.UseSpecularMap = value; return; }
    if (strcmp(n, "u_DiffuseTexture") == 0)  { return; } // texture binding, handled by TextureMTL::Bind
    if (strcmp(n, "u_SpecularMap") == 0)     { return; } // texture binding

    // Light array
    int idx = ParseArrayIndex(n, "u_LightType");
    if (idx >= 0) { m_FragmentUniforms.Lights[idx].type = value; return; }
}

void ShaderMTL::SetFloat(const CHEngine::String& name, float value)
{
    const char* n = name.c_str();

    if (strcmp(n, "u_Shininess") == 0) { m_FragmentUniforms.Shininess = value; return; }
    if (strcmp(n, "u_Selected") == 0)  { m_FragmentUniforms.Selected = value; return; }

    // Light array
    int idx;
    idx = ParseArrayIndex(n, "u_LightIntensity");
    if (idx >= 0) { m_FragmentUniforms.Lights[idx].colorIntensity[3] = value; return; }

    idx = ParseArrayIndex(n, "u_LightRange");
    if (idx >= 0) { m_FragmentUniforms.Lights[idx].range = value; return; }

    idx = ParseArrayIndex(n, "u_LightInnerCone");
    if (idx >= 0) { m_FragmentUniforms.Lights[idx].innerCone = value; return; }

    idx = ParseArrayIndex(n, "u_LightOuterCone");
    if (idx >= 0) { m_FragmentUniforms.Lights[idx].outerCone = value; return; }
}

void ShaderMTL::SetFloat3(const CHEngine::String& name, float x, float y, float z)
{
    const char* n = name.c_str();

    if (strcmp(n, "u_CameraPos") == 0) {
        m_FragmentUniforms.CameraPos[0] = x;
        m_FragmentUniforms.CameraPos[1] = y;
        m_FragmentUniforms.CameraPos[2] = z;
        return;
    }
    if (strcmp(n, "u_AmbientColor") == 0) {
        m_FragmentUniforms.AmbientColor[0] = x;
        m_FragmentUniforms.AmbientColor[1] = y;
        m_FragmentUniforms.AmbientColor[2] = z;
        return;
    }

    // Light arrays
    int idx;
    idx = ParseArrayIndex(n, "u_LightPosition");
    if (idx >= 0) {
        m_FragmentUniforms.Lights[idx].pos[0] = x;
        m_FragmentUniforms.Lights[idx].pos[1] = y;
        m_FragmentUniforms.Lights[idx].pos[2] = z;
        return;
    }
    idx = ParseArrayIndex(n, "u_LightDirection");
    if (idx >= 0) {
        m_FragmentUniforms.Lights[idx].dir[0] = x;
        m_FragmentUniforms.Lights[idx].dir[1] = y;
        m_FragmentUniforms.Lights[idx].dir[2] = z;
        return;
    }
    idx = ParseArrayIndex(n, "u_LightColor");
    if (idx >= 0) {
        m_FragmentUniforms.Lights[idx].colorIntensity[0] = x;
        m_FragmentUniforms.Lights[idx].colorIntensity[1] = y;
        m_FragmentUniforms.Lights[idx].colorIntensity[2] = z;
        return;
    }
}

void ShaderMTL::SetFloat4(const CHEngine::String& name, float x, float y, float z, float w)
{
    const char* n = name.c_str();

    if (strcmp(n, "u_Color") == 0) {
        m_FragmentUniforms.Color[0] = x;
        m_FragmentUniforms.Color[1] = y;
        m_FragmentUniforms.Color[2] = z;
        m_FragmentUniforms.Color[3] = w;
        return;
    }
}

void ShaderMTL::SetMat4(const CHEngine::String& name, const float* matrix)
{
    const char* n = name.c_str();

    if (strcmp(n, "u_ViewProjection") == 0) {
        memcpy(m_VertexUniforms.ViewProjection, matrix, 64);
        return;
    }
    if (strcmp(n, "u_Transform") == 0) {
        memcpy(m_VertexUniforms.Transform, matrix, 64);
        return;
    }
    if (strcmp(n, "u_NormalMatrix") == 0) {
        memcpy(m_VertexUniforms.NormalMatrix, matrix, 64);
        return;
    }
    if (strcmp(n, "u_InvViewProj") == 0) {
        memcpy(m_VertexUniforms.InvViewProj, matrix, 64);
        return;
    }
}

} // namespace CHModules
