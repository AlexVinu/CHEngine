#include "UnifiedBufferMTL.h"
#include "MetalGlobals.h"
#include <Log/Log.h>

#import <Metal/Metal.h>

namespace CHModules {

UnifiedBufferMTL::UnifiedBufferMTL(uint64_t size,
                                   CHEngine::BufferUsage usage,
                                   CHEngine::MemoryType  memory,
                                   std::span<const std::byte> initialData)
    : m_Size(size), m_Usage(usage), m_Memory(memory)
{
    id<MTLDevice> device = (id<MTLDevice>)MTLGlobals::g_Device;
    if (!device) {
        CHE_CORE_ERROR("UnifiedBufferMTL: Metal device is null");
        return;
    }

    MTLResourceOptions opts = MTLResourceStorageModeShared;
    if (memory == CHEngine::MemoryType::GpuOnly)
        opts = MTLResourceStorageModePrivate;

    id<MTLBuffer> buf = [device newBufferWithLength:(NSUInteger)size options:opts];
    if (!buf) {
        CHE_CORE_ERROR("UnifiedBufferMTL: failed to allocate {} bytes", size);
        return;
    }

    // Upload initial data if provided and buffer is CPU-accessible.
    if (!initialData.empty() && memory != CHEngine::MemoryType::GpuOnly) {
        const uint64_t copySize = std::min<uint64_t>(initialData.size_bytes(), size);
        std::memcpy([buf contents], initialData.data(), copySize);
    }

    m_Buffer = (void*)buf;
}

UnifiedBufferMTL::~UnifiedBufferMTL()
{
    if (m_Buffer) {
        [(id<MTLBuffer>)m_Buffer release];
        m_Buffer = nullptr;
    }
}

void UnifiedBufferMTL::Update(std::span<const std::byte> data, uint64_t offset)
{
    if (!m_Buffer || m_Memory == CHEngine::MemoryType::GpuOnly) return;
    id<MTLBuffer> buf = (id<MTLBuffer>)m_Buffer;

    const uint64_t copySize = std::min<uint64_t>(data.size_bytes(), m_Size - offset);
    if (copySize == 0) return;

    uint8_t* dst = static_cast<uint8_t*>([buf contents]) + offset;
    std::memcpy(dst, data.data(), copySize);
}

} // namespace CHModules
