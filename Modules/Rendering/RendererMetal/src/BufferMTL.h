#pragma once

#include <cstdint>
#include <Render/Pipeline/VertexLayout.h>

namespace CHModules
{
    // Legacy buffer classes for old EditorViewport path
    // Note: These don't inherit from IVertexBuffer/IIndexBuffer anymore
    // as those interfaces were removed in the new architecture
    
    class VertexBufferMTL
    {
    public:
        VertexBufferMTL(float* vertices, uint32_t size);
        ~VertexBufferMTL();

        void Bind() const;
        void Unbind() const;

        // Legacy methods (kept for compatibility)
        const CHEngine::VertexInputLayout& GetLayout() const;
        void SetLayout(const CHEngine::VertexInputLayout& layout);

        void* GetNativeBuffer() const { return m_Buffer; }
        uint32_t GetSize() const { return m_Size; }

    private:
        void* m_Buffer = nullptr; // id<MTLBuffer>
        uint32_t m_Size = 0;
        CHEngine::VertexInputLayout m_Layout;
    };

    class IndexBufferMTL
    {
    public:
        IndexBufferMTL(uint32_t* indices, uint32_t count);
        ~IndexBufferMTL();

        void Bind() const;
        void Unbind() const;
        uint32_t GetCount() const;

        void* GetNativeBuffer() const { return m_Buffer; }

    private:
        void* m_Buffer = nullptr; // id<MTLBuffer>
        uint32_t m_Count = 0;
    };
}
