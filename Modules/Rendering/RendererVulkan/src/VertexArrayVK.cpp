#include "VertexArrayVK.h"

namespace CHModules
{
    VertexArrayVK::VertexArrayVK() {}
    VertexArrayVK::~VertexArrayVK() {}


    void VertexArrayVK::AddVertexBuffer(const CHEngine::Ref<CHEngine::IVertexBuffer>& vertexBuffer)
    {
        m_VertexBuffers.push_back(vertexBuffer);
    }

    void VertexArrayVK::SetIndexBuffer(const CHEngine::Ref<CHEngine::IIndexBuffer>& indexBuffer)
    {
        m_IndexBuffer = indexBuffer;
    }

    const CHEngine::Vector<CHEngine::Ref<CHEngine::IVertexBuffer>>& VertexArrayVK::GetVertexBuffers() const
    {
        return m_VertexBuffers;
    }

    const CHEngine::Ref<CHEngine::IIndexBuffer>& VertexArrayVK::GetIndexBuffer() const
    {
        return m_IndexBuffer;
    }
}
