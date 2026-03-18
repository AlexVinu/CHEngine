#include <cstdint>
#include "BufferMTL.h"
#include "MetalGlobals.h"

#include <Log/Log.h>

#import <Metal/Metal.h>

namespace CHModules
{

// ─── Vertex Buffer ──────────────────────────────────────────────────────────

VertexBufferMTL::VertexBufferMTL(float* vertices, uint32_t size)
    : m_Size(size)
{
    id<MTLDevice> device = (id<MTLDevice>)MTLGlobals::g_Device;
    if (!device || !vertices || size == 0) {
        CHE_CORE_ERROR("VertexBufferMTL: invalid args (device={}, vertices={}, size={})",
                       device != nil, vertices != nullptr, size);
        return;
    }

    id<MTLBuffer> buffer = [device newBufferWithBytes:vertices
                                               length:size
                                              options:MTLResourceStorageModeShared];
    m_Buffer = (void*)buffer;
}

VertexBufferMTL::~VertexBufferMTL()
{
    if (m_Buffer)
        [(id<MTLBuffer>)m_Buffer release];
}

void VertexBufferMTL::Bind() const
{
    // В Metal буферы привязываются через encoder в DrawIndexed
}

void VertexBufferMTL::Unbind() const {}

const CHEngine::BufferLayout& VertexBufferMTL::GetLayout() const { return m_Layout; }
void VertexBufferMTL::SetLayout(const CHEngine::BufferLayout& layout) { m_Layout = layout; }

// ─── Index Buffer ───────────────────────────────────────────────────────────

IndexBufferMTL::IndexBufferMTL(uint32_t* indices, uint32_t count)
    : m_Count(count)
{
    id<MTLDevice> device = (id<MTLDevice>)MTLGlobals::g_Device;
    if (!device || !indices || count == 0) {
        CHE_CORE_ERROR("IndexBufferMTL: invalid args");
        return;
    }

    id<MTLBuffer> buffer = [device newBufferWithBytes:indices
                                               length:count * sizeof(uint32_t)
                                              options:MTLResourceStorageModeShared];
    m_Buffer = (void*)buffer;
}

IndexBufferMTL::~IndexBufferMTL()
{
    if (m_Buffer)
        [(id<MTLBuffer>)m_Buffer release];
}

void IndexBufferMTL::Bind() const {}
void IndexBufferMTL::Unbind() const {}
uint32_t IndexBufferMTL::GetCount() const { return m_Count; }

} // namespace CHModules
