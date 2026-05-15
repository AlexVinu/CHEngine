#include "chepch.h"
#include "VertexArrayMTL.h"

namespace CHModules
{
    VertexArrayMTL::VertexArrayMTL() {}
    VertexArrayMTL::~VertexArrayMTL() {}
    void VertexArrayMTL::Bind() const {}
    void VertexArrayMTL::Unbind() const {}

    void VertexArrayMTL::AddVertexBuffer(class VertexBufferMTL* vb)
    { m_VertexBuffers.push_back(vb); }

    void VertexArrayMTL::SetIndexBuffer(class IndexBufferMTL* ib)
    { m_IndexBuffer = ib; }

    const Vector<class VertexBufferMTL*>& VertexArrayMTL::GetVertexBuffers() const
    { return m_VertexBuffers; }

    class IndexBufferMTL* VertexArrayMTL::GetIndexBuffer() const
    { return m_IndexBuffer; }
}
