#include "BufferVK.h"

#include <Log/Log.h>

namespace CHModules
{
    // ─── VertexBuffer ─────────────────────────────────────────────────────────

    VertexBufferVK::VertexBufferVK(float* /*vertices*/, uint32_t /*size*/)
    {
        // TODO: создать VkBuffer + VkDeviceMemory, скопировать данные
    }

    VertexBufferVK::~VertexBufferVK()
    {
        // TODO: vkDestroyBuffer, vkFreeMemory
    }

    void VertexBufferVK::Bind() const {}
    void VertexBufferVK::Unbind() const {}

    const CHEngine::BufferLayout& VertexBufferVK::GetLayout() const { return m_Layout; }
    void VertexBufferVK::SetLayout(const CHEngine::BufferLayout& layout) { m_Layout = layout; }

    // ─── IndexBuffer ──────────────────────────────────────────────────────────

    IndexBufferVK::IndexBufferVK(uint32_t* /*indices*/, uint32_t count)
        : m_Count(count)
    {
        // TODO: создать VkBuffer + VkDeviceMemory, скопировать данные
    }

    IndexBufferVK::~IndexBufferVK()
    {
        // TODO: vkDestroyBuffer, vkFreeMemory
    }

    void IndexBufferVK::Bind() const {}
    void IndexBufferVK::Unbind() const {}
    uint32_t IndexBufferVK::GetCount() const { return m_Count; }
}
