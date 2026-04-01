#pragma once

#include "Render/IVertexArray.h"

namespace CHModules {

    class VertexArrayMTL : public CHEngine::IVertexArray
    {
    public:
        VertexArrayMTL();
        ~VertexArrayMTL() override;

        void Bind() const override;
        void Unbind() const override;

        void AddVertexBuffer(const Ref<CHEngine::IVertexBuffer>& vertexBuffer) override;
        void SetIndexBuffer(const Ref<CHEngine::IIndexBuffer>& indexBuffer) override;

        const CHEngine::Vector<Ref<CHEngine::IVertexBuffer>>& GetVertexBuffers() const override;
        const Ref<CHEngine::IIndexBuffer>& GetIndexBuffer() const override;

    private:
        CHEngine::Vector<Ref<CHEngine::IVertexBuffer>> m_VertexBuffers;
        Ref<CHEngine::IIndexBuffer> m_IndexBuffer;
    };

}
