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

        void AddVertexBuffer(const std::shared_ptr<CHEngine::IVertexBuffer>& vertexBuffer) override;
        void SetIndexBuffer(const std::shared_ptr<CHEngine::IIndexBuffer>& indexBuffer) override;

        const CHEngine::Vector<std::shared_ptr<CHEngine::IVertexBuffer>>& GetVertexBuffers() const override;
        const std::shared_ptr<CHEngine::IIndexBuffer>& GetIndexBuffer() const override;

    private:
        CHEngine::Vector<std::shared_ptr<CHEngine::IVertexBuffer>> m_VertexBuffers;
        std::shared_ptr<CHEngine::IIndexBuffer> m_IndexBuffer;
    };

}
