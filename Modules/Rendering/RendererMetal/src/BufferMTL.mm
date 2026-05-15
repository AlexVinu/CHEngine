#include <cstdint>
#include "BufferMTL.h"
#include "MetalGlobals.h"

#include <Log/Log.h>
#include <Render/Pipeline/VertexLayout.h>

#import <Metal/Metal.h>

namespace CHModules
{

// ─── Vertex Buffer ──────────────────────────────────────────────────────────

CHModules::VertexBufferMTL::VertexBufferMTL(float* vertices, uint32_t size)
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

CHModules::VertexBufferMTL::~VertexBufferMTL()
{
    if (m_Buffer)
        [(id<MTLBuffer>)m_Buffer release];
}

void CHModules::VertexBufferMTL::Bind() const
{
    // В Metal буферы привязываются через encoder в DrawIndexed
}

void CHModules::VertexBufferMTL::Unbind() const {}

const CHEngine::VertexInputLayout& CHModules::VertexBufferMTL::GetLayout() const { return m_Layout; }
void CHModules::VertexBufferMTL::SetLayout(const CHEngine::VertexInputLayout& layout) { m_Layout = layout; }

// ─── Index Buffer ───────────────────────────────────────────────────────────

CHModules::IndexBufferMTL::IndexBufferMTL(uint32_t* indices, uint32_t count)
    : m_Count(count)
{
    id<MTLDevice> device = (id<MTLDevice>)MTLGlobals::g_Device;
    if (!device || !indices || count == 0) {
        CHE_CORE_ERROR("IndexBufferMTL: invalid args");
        return;
    }

    const size_t size = count * sizeof(uint32_t);
    id<MTLBuffer> buffer = [device newBufferWithBytes:indices
                                               length:size
                                              options:MTLResourceStorageModeShared];
    m_Buffer = (void*)buffer;
}

CHModules::IndexBufferMTL::~IndexBufferMTL()
{
    if (m_Buffer)
        [(id<MTLBuffer>)m_Buffer release];
}

void CHModules::IndexBufferMTL::Bind() const {}

void CHModules::IndexBufferMTL::Unbind() const {}

uint32_t CHModules::IndexBufferMTL::GetCount() const { return m_Count; }

} // namespace CHModules

