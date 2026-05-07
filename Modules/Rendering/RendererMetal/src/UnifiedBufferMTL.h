#pragma once

#include "Render/Core/RenderTypes.h"
#include <span>
#include <cstdint>

namespace CHModules {

    /// Metal implementation of a generic GPU buffer.
    /// Covers vertex, index, and uniform buffers in a single class.
    class UnifiedBufferMTL {
    public:
        UnifiedBufferMTL(uint64_t size,
                         CHEngine::BufferUsage usage,
                         CHEngine::MemoryType  memory,
                         std::span<const std::byte> initialData);
        ~UnifiedBufferMTL();

        UnifiedBufferMTL(const UnifiedBufferMTL&)            = delete;
        UnifiedBufferMTL& operator=(const UnifiedBufferMTL&) = delete;

        /// Upload data into the buffer (CpuToGpu only).
        void Update(std::span<const std::byte> data, uint64_t offset = 0);

        void*    GetNative() const { return m_Buffer; }   // id<MTLBuffer>
        uint64_t GetSize()   const { return m_Size; }
        CHEngine::BufferUsage GetUsage() const { return m_Usage; }

    private:
        void* m_Buffer = nullptr; // id<MTLBuffer>
        uint64_t m_Size = 0;
        CHEngine::BufferUsage m_Usage;
        CHEngine::MemoryType  m_Memory;
    };

} // namespace CHModules
