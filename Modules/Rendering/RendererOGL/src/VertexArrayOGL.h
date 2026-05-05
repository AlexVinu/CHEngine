#pragma once

#include "Render/Pipeline/VertexLayout.h"
#include "Render/Core/RenderTypes.h"
#include "Render/Handles.h"

#include <glad/glad.h>
#include <cstdint>

namespace CHModules {

    class BufferOGL;

    /// Handle-based VAO. Owns a GL VAO object.
    /// Created by RenderFactoryOGL::CreateVertexArray.
    class VertexArrayOGL
    {
    public:
        VertexArrayOGL(const CHEngine::VertexInputLayout& layout,
                       BufferOGL* vertexBuffer,
                       BufferOGL* indexBuffer,    // may be nullptr for non-indexed draws
                       CHEngine::IndexFormat indexFormat,
                       uint32_t indexCount);

        ~VertexArrayOGL();

        VertexArrayOGL(const VertexArrayOGL&) = delete;
        VertexArrayOGL& operator=(const VertexArrayOGL&) = delete;

        void Bind()   const;
        void Unbind() const;

        CHEngine::IndexFormat GetIndexFormat() const { return m_IndexFormat; }
        uint32_t              GetIndexCount()  const { return m_IndexCount; }

        bool HasIndexBuffer() const { return m_IndexBuffer != nullptr; }

    private:
        GLuint m_VAO = 0;

        BufferOGL* m_VertexBuffer = nullptr;
        BufferOGL* m_IndexBuffer  = nullptr;

        CHEngine::IndexFormat m_IndexFormat = CHEngine::IndexFormat::UInt32;
        uint32_t              m_IndexCount  = 0;
    };

} // namespace CHModules
