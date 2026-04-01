#include "chepch.h"
#include "VertexArrayMTL.h"

namespace CHModules
{
    VertexArrayMTL::VertexArrayMTL() {}
    VertexArrayMTL::~VertexArrayMTL() {}
    void VertexArrayMTL::Bind() const {}
    void VertexArrayMTL::Unbind() const {}

    void VertexArrayMTL::AddVertexBuffer(const Ref<CHEngine::IVertexBuffer>& vb)
    { m_VertexBuffers.push_back(vb); }

    void VertexArrayMTL::SetIndexBuffer(const Ref<CHEngine::IIndexBuffer>& ib)
    { m_IndexBuffer = ib; }

    const CHEngine::Vector<Ref<CHEngine::IVertexBuffer>>& VertexArrayMTL::GetVertexBuffers() const
    { return m_VertexBuffers; }

    const Ref<CHEngine::IIndexBuffer>& VertexArrayMTL::GetIndexBuffer() const
    { return m_IndexBuffer; }
}
