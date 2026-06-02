#include "BufferVK.h"
#include "VulkanContext.h"
#include "VulkanGlobals.h"

#include <Log/Log.h>
#include <cstring>

namespace CHModules {

    namespace {
        VulkanContext* GetCtx() {
            return static_cast<VulkanContext*>(VKGlobals::g_ContextPtr);
        }

        VkBufferUsageFlags ToVkUsage(CHEngine::BufferUsage usage)
        {
            VkBufferUsageFlags flags = 0;
            if (static_cast<uint32_t>(usage) & static_cast<uint32_t>(CHEngine::BufferUsage::Vertex))
                flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            if (static_cast<uint32_t>(usage) & static_cast<uint32_t>(CHEngine::BufferUsage::Index))
                flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            if (static_cast<uint32_t>(usage) & static_cast<uint32_t>(CHEngine::BufferUsage::Uniform))
                flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            if (static_cast<uint32_t>(usage) & static_cast<uint32_t>(CHEngine::BufferUsage::Storage))
                flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            if (static_cast<uint32_t>(usage) & static_cast<uint32_t>(CHEngine::BufferUsage::Indirect))
                flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
            return flags;
        }
    }

    BufferVK::BufferVK(uint64_t size,
                       CHEngine::BufferUsage usage,
                       CHEngine::MemoryType  memory,
                       std::span<const std::byte> initialData)
        : m_Size(size), m_Usage(usage), m_Memory(memory)
    {
        VulkanContext* ctx = GetCtx();
        if (!ctx) { CHE_CORE_ERROR("BufferVK: no VulkanContext"); return; }

        VkBufferUsageFlags vkUsage = ToVkUsage(usage);
        // GPU-only buffers also need TRANSFER_DST for staging upload.
        if (memory == CHEngine::MemoryType::GpuOnly)
            vkUsage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VmaAllocationCreateInfo vmaInfo = {};

        if (memory == CHEngine::MemoryType::GpuOnly) {
            vmaInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
            m_HostVisible = false;
        } else if (memory == CHEngine::MemoryType::CpuToGpu) {
            vmaInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
            m_HostVisible = true;
        } else {
            vmaInfo.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
            m_HostVisible = true;
        }

        VkBufferCreateInfo bufInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size  = size,
            .usage = vkUsage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };

        VkResult res = vmaCreateBuffer(ctx->GetAllocator(), &bufInfo, &vmaInfo, &m_Buffer, &m_Alloc, nullptr);
        if (res != VK_SUCCESS) {
            CHE_CORE_ERROR("BufferVK: vmaCreateBuffer failed");
            return;
        }

        if (!initialData.empty())
            UpdateData(initialData, 0);
    }

    BufferVK::~BufferVK()
    {
        VulkanContext* ctx = GetCtx();
        if (ctx && m_Buffer != VK_NULL_HANDLE)
            vmaDestroyBuffer(ctx->GetAllocator(), m_Buffer, m_Alloc);
        m_Buffer = VK_NULL_HANDLE;
        m_Alloc  = VK_NULL_HANDLE;
    }

    void BufferVK::UpdateData(std::span<const std::byte> data, uint64_t offset)
    {
        if (data.empty()) return;
        VulkanContext* ctx = GetCtx();
        if (!ctx || m_Buffer == VK_NULL_HANDLE) return;

        const uint64_t writeSize = data.size();

        if (m_HostVisible) {
            void* mapped = nullptr;
            const VkResult mr = vmaMapMemory(ctx->GetAllocator(), m_Alloc, &mapped);
            if (mr != VK_SUCCESS || !mapped) {
                CHE_CORE_ERROR("BufferVK::UpdateData: vmaMapMemory failed (VkResult={})", static_cast<int>(mr));
                return;
            }
            std::memcpy(static_cast<uint8_t*>(mapped) + offset, data.data(), writeSize);
            vmaUnmapMemory(ctx->GetAllocator(), m_Alloc);
        } else {
            // GPU-only: use a staging buffer.
            VkBufferCreateInfo stagingInfo = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size  = writeSize,
                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            };
            VmaAllocationCreateInfo stagingVmaInfo = {};
            stagingVmaInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

            VkBuffer      stagingBuf  = VK_NULL_HANDLE;
            VmaAllocation stagingAlloc = VK_NULL_HANDLE;
            if (vmaCreateBuffer(ctx->GetAllocator(), &stagingInfo, &stagingVmaInfo,
                                &stagingBuf, &stagingAlloc, nullptr) != VK_SUCCESS) {
                CHE_CORE_ERROR("BufferVK::UpdateData: staging vmaCreateBuffer failed");
                return;
            }

            void* mapped = nullptr;
            const VkResult mr = vmaMapMemory(ctx->GetAllocator(), stagingAlloc, &mapped);
            if (mr != VK_SUCCESS || !mapped) {
                CHE_CORE_ERROR("BufferVK::UpdateData: staging vmaMapMemory failed (VkResult={})", static_cast<int>(mr));
                vmaDestroyBuffer(ctx->GetAllocator(), stagingBuf, stagingAlloc);
                return;
            }
            std::memcpy(mapped, data.data(), writeSize);
            vmaUnmapMemory(ctx->GetAllocator(), stagingAlloc);

            VkCommandBuffer cmd = ctx->BeginSingleTimeCommands();
            VkBufferCopy region = { .srcOffset = 0, .dstOffset = offset, .size = writeSize };
            vkCmdCopyBuffer(cmd, stagingBuf, m_Buffer, 1, &region);
            ctx->EndSingleTimeCommands(cmd);

            vmaDestroyBuffer(ctx->GetAllocator(), stagingBuf, stagingAlloc);
        }
    }

} // namespace CHModules
