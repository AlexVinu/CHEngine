#include <chepch.h>
#include "VertexArrayOGL.h"
#include "BufferOGL.h"

#include <Log/Log.h>

namespace CHModules {

    namespace {

        struct GLAttribInfo {
            GLenum    type;
            GLint     components;
            GLboolean normalized;
        };

        GLAttribInfo GetGLAttribInfo(CHEngine::VertexFormat fmt)
        {
            using VF = CHEngine::VertexFormat;
            switch (fmt)
            {
            case VF::Float:        return { GL_FLOAT,         1, GL_FALSE };
            case VF::Float2:       return { GL_FLOAT,         2, GL_FALSE };
            case VF::Float3:       return { GL_FLOAT,         3, GL_FALSE };
            case VF::Float4:       return { GL_FLOAT,         4, GL_FALSE };
            case VF::Int:          return { GL_INT,           1, GL_FALSE };
            case VF::Int2:         return { GL_INT,           2, GL_FALSE };
            case VF::Int3:         return { GL_INT,           3, GL_FALSE };
            case VF::Int4:         return { GL_INT,           4, GL_FALSE };
            case VF::UInt:         return { GL_UNSIGNED_INT,  1, GL_FALSE };
            case VF::UInt2:        return { GL_UNSIGNED_INT,  2, GL_FALSE };
            case VF::UInt3:        return { GL_UNSIGNED_INT,  3, GL_FALSE };
            case VF::UInt4:        return { GL_UNSIGNED_INT,  4, GL_FALSE };
            case VF::UByte4:       return { GL_UNSIGNED_BYTE, 4, GL_FALSE };
            case VF::UByte4_Norm:  return { GL_UNSIGNED_BYTE, 4, GL_TRUE  };
            default:
                CHE_CORE_WARN("VertexArrayOGL: unsupported VertexFormat {}", (int)fmt);
                return { GL_FLOAT, 1, GL_FALSE };
            }
        }

    } // anonymous namespace

    VertexArrayOGL::VertexArrayOGL(const CHEngine::VertexInputLayout& layout,
                                   BufferOGL* vertexBuffer,
                                   BufferOGL* indexBuffer,
                                   CHEngine::IndexFormat indexFormat,
                                   uint32_t indexCount)
        : m_VertexBuffer(vertexBuffer)
        , m_IndexBuffer(indexBuffer)
        , m_IndexFormat(indexFormat)
        , m_IndexCount(indexCount)
    {
        glGenVertexArrays(1, &m_VAO);
        glBindVertexArray(m_VAO);

        if (m_VertexBuffer)
        {
            glBindBuffer(GL_ARRAY_BUFFER, m_VertexBuffer->GetID());

            uint32_t attribIndex = 0;
            for (const auto& attr : layout.Attributes)
            {
                const uint32_t slotIdx = (attr.Slot < static_cast<uint32_t>(layout.Strides.size()))
                                             ? attr.Slot : 0u;
                const uint32_t stride  = layout.Strides.empty() ? 0u : layout.Strides[slotIdx];

                GLAttribInfo info = GetGLAttribInfo(attr.Format);

                glEnableVertexAttribArray(attribIndex);

                if (info.type == GL_INT || info.type == GL_UNSIGNED_INT)
                {
                    glVertexAttribIPointer(attribIndex,
                                          info.components,
                                          info.type,
                                          static_cast<GLsizei>(stride),
                                          reinterpret_cast<const void*>(static_cast<uintptr_t>(attr.Offset)));
                }
                else
                {
                    glVertexAttribPointer(attribIndex,
                                         info.components,
                                         info.type,
                                         info.normalized,
                                         static_cast<GLsizei>(stride),
                                         reinterpret_cast<const void*>(static_cast<uintptr_t>(attr.Offset)));
                }

                ++attribIndex;
            }
        }

        if (m_IndexBuffer)
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IndexBuffer->GetID());

        glBindVertexArray(0);
    }

    VertexArrayOGL::~VertexArrayOGL()
    {
        if (m_VAO)
        {
            glDeleteVertexArrays(1, &m_VAO);
            m_VAO = 0;
        }
    }

    void VertexArrayOGL::Bind() const
    {
        glBindVertexArray(m_VAO);
    }

    void VertexArrayOGL::Unbind() const
    {
        glBindVertexArray(0);
    }

} // namespace CHModules
