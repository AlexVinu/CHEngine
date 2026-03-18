#include "VertexArrayVK.h"

namespace CHModules
{
    VertexArrayVK::VertexArrayVK() {}
    VertexArrayVK::~VertexArrayVK() {}

    void VertexArrayVK::Bind() const {}
    void VertexArrayVK::Unbind() const {}

    void VertexArrayVK::AddVertexBuffer(const std::shared_ptr<CHEngine::IVertexBuffer>& vertexBuffer)
    {
        m_VertexBuffers.push_back(vertexBuffer);
    }

    void VertexArrayVK::SetIndexBuffer(const std::shared_ptr<CHEngine::IIndexBuffer>& indexBuffer)
    {
        m_IndexBuffer = indexBuffer;
    }

    const CHEngine::Vector<std::shared_ptr<CHEngine::IVertexBuffer>>& VertexArrayVK::GetVertexBuffers() const
    {
        return m_VertexBuffers;
    }

    const std::shared_ptr<CHEngine::IIndexBuffer>& VertexArrayVK::GetIndexBuffer() const
    {
        return m_IndexBuffer;
    }
}
