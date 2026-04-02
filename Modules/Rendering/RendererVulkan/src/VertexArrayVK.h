#pragma once

#include "Render/IVertexArray.h"

namespace CHModules {

    class VertexArrayVK : public CHEngine::IVertexArray
    {
    public:
        VertexArrayVK();
        ~VertexArrayVK() override;

        void Bind() const override;
        void Unbind() const override;

        void AddVertexBuffer(const CHEngine::Ref<CHEngine::IVertexBuffer>& vertexBuffer) override;
        void SetIndexBuffer(const CHEngine::Ref<CHEngine::IIndexBuffer>& indexBuffer) override;

        const CHEngine::Vector<CHEngine::Ref<CHEngine::IVertexBuffer>>& GetVertexBuffers() const override;
        const CHEngine::Ref<CHEngine::IIndexBuffer>& GetIndexBuffer() const override;

    private:
        CHEngine::Vector<CHEngine::Ref<CHEngine::IVertexBuffer>> m_VertexBuffers;
        CHEngine::Ref<CHEngine::IIndexBuffer> m_IndexBuffer;
    };

}
