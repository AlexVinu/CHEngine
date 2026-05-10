#include "chepch.h"
#include "VertexArrayMTL.h"

namespace CHModules
{
    VertexArrayMTL::VertexArrayMTL() {}
    VertexArrayMTL::~VertexArrayMTL() {}
    void VertexArrayMTL::Bind() const {}
    void VertexArrayMTL::Unbind() const {}

    void VertexArrayMTL::AddVertexBuffer(const CHEngine::Ref<CHEngine::IVertexBuffer>& vb)
    { m_VertexBuffers.push_back(vb); }

    void VertexArrayMTL::SetIndexBuffer(const CHEngine::Ref<CHEngine::IIndexBuffer>& ib)
    { m_IndexBuffer = ib; }

    const CHEngine::Vector<CHEngine::Ref<CHEngine::IVertexBuffer>>& VertexArrayMTL::GetVertexBuffers() const
    { return m_VertexBuffers; }

    const CHEngine::Ref<CHEngine::IIndexBuffer>& VertexArrayMTL::GetIndexBuffer() const
    { return m_IndexBuffer; }
}
