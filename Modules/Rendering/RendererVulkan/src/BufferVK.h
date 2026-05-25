#pragma once

#include "Render/Core/RenderTypes.h"
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <span>
#include <cstdint>

namespace CHModules {

    class BufferVK
    {
    public:
        BufferVK(uint64_t size,
                 CHEngine::BufferUsage usage,
                 CHEngine::MemoryType  memory,
                 std::span<const std::byte> initialData);
        ~BufferVK();

        BufferVK(const BufferVK&)            = delete;
        BufferVK& operator=(const BufferVK&) = delete;

        void UpdateData(std::span<const std::byte> data, uint64_t offset = 0);

        VkBuffer     GetBuffer() const { return m_Buffer; }
        uint64_t     GetSize()   const { return m_Size; }
        CHEngine::BufferUsage GetUsage()  const { return m_Usage; }
        CHEngine::MemoryType  GetMemory() const { return m_Memory; }

    private:
        VkBuffer      m_Buffer = VK_NULL_HANDLE;
        VmaAllocation m_Alloc  = VK_NULL_HANDLE;
        uint64_t      m_Size   = 0;
        CHEngine::BufferUsage m_Usage;
        CHEngine::MemoryType  m_Memory;

        bool m_HostVisible = false; // true for CpuToGpu — can map directly
    };

} // namespace CHModules
