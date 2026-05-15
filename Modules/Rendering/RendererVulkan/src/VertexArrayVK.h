#pragma once

#include "Render/IVertexArray.h"

namespace CHModules {

    class VertexArrayVK : public CHEngine::IVertexArray
    {
    public:
        VertexArrayVK();
        ~VertexArrayVK() override;

        void AddVertexBuffer(const Ref<CHEngine::IVertexBuffer>& vertexBuffer) override;
        void SetIndexBuffer(const Ref<CHEngine::IIndexBuffer>& indexBuffer) override;

        const Vector<Ref<CHEngine::IVertexBuffer>>& GetVertexBuffers() const override;
        const Ref<CHEngine::IIndexBuffer>& GetIndexBuffer() const override;

    private:
        Vector<Ref<CHEngine::IVertexBuffer>> m_VertexBuffers;
        Ref<CHEngine::IIndexBuffer> m_IndexBuffer;
    };

}
