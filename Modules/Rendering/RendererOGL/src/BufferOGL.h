#pragma once

#include "Render/IBuffer.h"

namespace CHModules
{
    class VertexBufferOGL : public CHEngine::IVertexBuffer
    {
    public:
        VertexBufferOGL(float* vertices, uint32_t size);
        virtual ~VertexBufferOGL() override;

        virtual void Bind() const override;
        virtual void Unbind() const override;

        virtual const CHEngine::BufferLayout& GetLayout() const override;
        virtual void SetLayout(const CHEngine::BufferLayout& layout) override;
    protected:
        uint32_t m_RendererID;
        CHEngine::BufferLayout m_Layout;
    };

    class IndexBufferOGL : public CHEngine::IIndexBuffer
    {
    public:
        IndexBufferOGL(uint32_t* indices, uint32_t count);
        virtual ~IndexBufferOGL() override;

        virtual void Bind() const override;
        virtual void Unbind() const override;

        virtual uint32_t GetCount() const override;
    protected:
        uint32_t m_RendererID;
        uint32_t m_Count;
    };
}
