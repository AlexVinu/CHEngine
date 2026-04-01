#include "VertexArrayVK.h"

namespace CHModules
{
    VertexArrayVK::VertexArrayVK() {}
    VertexArrayVK::~VertexArrayVK() {}

    void VertexArrayVK::Bind() const {}
    void VertexArrayVK::Unbind() const {}

    void VertexArrayVK::AddVertexBuffer(const Ref<CHEngine::IVertexBuffer>& vertexBuffer)
    {
        m_VertexBuffers.push_back(vertexBuffer);
    }

    void VertexArrayVK::SetIndexBuffer(const Ref<CHEngine::IIndexBuffer>& indexBuffer)
    {
        m_IndexBuffer = indexBuffer;
    }

    const CHEngine::Vector<Ref<CHEngine::IVertexBuffer>>& VertexArrayVK::GetVertexBuffers() const
    {
        return m_VertexBuffers;
    }

    const Ref<CHEngine::IIndexBuffer>& VertexArrayVK::GetIndexBuffer() const
    {
        return m_IndexBuffer;
    }
}
