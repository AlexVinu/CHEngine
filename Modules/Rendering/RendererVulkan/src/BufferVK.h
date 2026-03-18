#pragma once

#include "Render/IBuffer.h"

namespace CHModules
{
    class VertexBufferVK : public CHEngine::IVertexBuffer
    {
    public:
        VertexBufferVK(float* vertices, uint32_t size);
        ~VertexBufferVK() override;

        void Bind() const override;
        void Unbind() const override;

        const CHEngine::BufferLayout& GetLayout() const override;
        void SetLayout(const CHEngine::BufferLayout& layout) override;

    private:
        CHEngine::BufferLayout m_Layout;
        // TODO: VkBuffer + VkDeviceMemory
    };

    class IndexBufferVK : public CHEngine::IIndexBuffer
    {
    public:
        IndexBufferVK(uint32_t* indices, uint32_t count);
        ~IndexBufferVK() override;

        void Bind() const override;
        void Unbind() const override;
        uint32_t GetCount() const override;

    private:
        uint32_t m_Count = 0;
        // TODO: VkBuffer + VkDeviceMemory
    };
}
