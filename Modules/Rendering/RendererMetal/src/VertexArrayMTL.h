#pragma once

#include <CheStl/Vector.h>
#include <Core.h>

namespace CHModules {

    // Legacy vertex array for old EditorViewport path
    // Note: Doesn't inherit from IVertexArray anymore as that interface was removed
    
    class VertexArrayMTL
    {
    public:
        VertexArrayMTL();
        ~VertexArrayMTL();

        void Bind() const;
        void Unbind() const;

        // These methods are kept for legacy compatibility but may not be used
        // in the new architecture
        void AddVertexBuffer(class VertexBufferMTL* vertexBuffer);
        void SetIndexBuffer(class IndexBufferMTL* indexBuffer);

        const CHEngine::Vector<class VertexBufferMTL*>& GetVertexBuffers() const;
        class IndexBufferMTL* GetIndexBuffer() const;

    private:
        CHEngine::Vector<class VertexBufferMTL*> m_VertexBuffers;
        class IndexBufferMTL* m_IndexBuffer;
    };

}
