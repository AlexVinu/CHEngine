#pragma once

#include "Render/IBuffer.h"

namespace CHModules
{
    class VertexBufferMTL : public CHEngine::IVertexBuffer
    {
    public:
        VertexBufferMTL(float* vertices, uint32_t size);
        ~VertexBufferMTL() override;

        void Bind() const override;
        void Unbind() const override;

        const CHEngine::BufferLayout& GetLayout() const override;
        void SetLayout(const CHEngine::BufferLayout& layout) override;

        void* GetNativeBuffer() const { return m_Buffer; }
        uint32_t GetSize() const { return m_Size; }

    private:
        void* m_Buffer = nullptr; // id<MTLBuffer>
        uint32_t m_Size = 0;
        CHEngine::BufferLayout m_Layout;
    };

    class IndexBufferMTL : public CHEngine::IIndexBuffer
    {
    public:
        IndexBufferMTL(uint32_t* indices, uint32_t count);
        ~IndexBufferMTL() override;

        void Bind() const override;
        void Unbind() const override;
        uint32_t GetCount() const override;

        void* GetNativeBuffer() const { return m_Buffer; }

    private:
        void* m_Buffer = nullptr; // id<MTLBuffer>
        uint32_t m_Count = 0;
    };
}
