#include <chepch.h>
#include "VertexArrayOGL.h"

#include <Profiler.h>
#include <glad/glad.h>

namespace CHModules
{
	static GLenum ShaderDataTypeToOpenGLBaseType(CHEngine::ShaderDataType type)
	{
		switch (type)
		{
		case CHEngine::ShaderDataType::Float:    return GL_FLOAT;
		case CHEngine::ShaderDataType::Float2:   return GL_FLOAT;
		case CHEngine::ShaderDataType::Float3:   return GL_FLOAT;
		case CHEngine::ShaderDataType::Float4:   return GL_FLOAT;
		case CHEngine::ShaderDataType::Mat3:     return GL_FLOAT;
		case CHEngine::ShaderDataType::Mat4:     return GL_FLOAT;
		case CHEngine::ShaderDataType::Int:      return GL_INT;
		case CHEngine::ShaderDataType::Int2:     return GL_INT;
		case CHEngine::ShaderDataType::Int3:     return GL_INT;
		case CHEngine::ShaderDataType::Int4:     return GL_INT;
		case CHEngine::ShaderDataType::Bool:     return GL_BOOL;
		}

		CHE_CORE_ASSERT(false, "Unknown ShaderDataType!");
		return 0;
	}

	VertexArrayOGL::VertexArrayOGL()
	{

		glGenVertexArrays(1, &m_RendererID);
	}

	VertexArrayOGL::~VertexArrayOGL()
	{

		glDeleteVertexArrays(1, &m_RendererID);
	}

	void VertexArrayOGL::Bind() const
	{

		glBindVertexArray(m_RendererID);
	}

	void VertexArrayOGL::Unbind() const
	{

		glBindVertexArray(0);
	}

	void VertexArrayOGL::AddVertexBuffer(const Ref<CHEngine::IVertexBuffer>& vertexBuffer)
	{

		CHE_CORE_ASSERT(vertexBuffer->GetLayout().GetElements().size(), "Vertex Buffer has no layout!");

		glBindVertexArray(m_RendererID);
		vertexBuffer->Bind();

		const auto& layout = vertexBuffer->GetLayout();
		for (const auto& element : layout)
		{
			switch (element.Type)
			{
			case CHEngine::ShaderDataType::Float:
			case CHEngine::ShaderDataType::Float2:
			case CHEngine::ShaderDataType::Float3:
			case CHEngine::ShaderDataType::Float4:
			{
				glEnableVertexAttribArray(m_VertexBufferIndex);
				glVertexAttribPointer(m_VertexBufferIndex,
					element.GetComponentCount(),
					ShaderDataTypeToOpenGLBaseType(element.Type),
					element.Normalized ? GL_TRUE : GL_FALSE,
					layout.GetStride(),
					(const void*)element.Offset);
				m_VertexBufferIndex++;
				break;
			}
			case CHEngine::ShaderDataType::Int:
			case CHEngine::ShaderDataType::Int2:
			case CHEngine::ShaderDataType::Int3:
			case CHEngine::ShaderDataType::Int4:
			case CHEngine::ShaderDataType::Bool:
			{
				glEnableVertexAttribArray(m_VertexBufferIndex);
				glVertexAttribIPointer(m_VertexBufferIndex,
					element.GetComponentCount(),
					ShaderDataTypeToOpenGLBaseType(element.Type),
					layout.GetStride(),
					(const void*)element.Offset);
				m_VertexBufferIndex++;
				break;
			}
			case CHEngine::ShaderDataType::Mat3:
			case CHEngine::ShaderDataType::Mat4:
			{
				uint8_t count = element.GetComponentCount();
				for (uint8_t i = 0; i < count; i++)
				{
					glEnableVertexAttribArray(m_VertexBufferIndex);
					glVertexAttribPointer(m_VertexBufferIndex,
						count,
						ShaderDataTypeToOpenGLBaseType(element.Type),
						element.Normalized ? GL_TRUE : GL_FALSE,
						layout.GetStride(),
						(const void*)(element.Offset + sizeof(float) * count * i));
					glVertexAttribDivisor(m_VertexBufferIndex, 1);
					m_VertexBufferIndex++;
				}
				break;
			}
			default:
				CHE_CORE_ASSERT(false, "Unknown ShaderDataType!");
			}
		}

		m_VertexBuffers.push_back(vertexBuffer);
	}

	void VertexArrayOGL::SetIndexBuffer(const Ref<CHEngine::IIndexBuffer>& indexBuffer)
	{

		glBindVertexArray(m_RendererID);
		indexBuffer->Bind();

		m_IndexBuffer = indexBuffer;
	}
	const CHEngine::Vector<Ref<CHEngine::IVertexBuffer>>& VertexArrayOGL::GetVertexBuffers() const
	{
		return m_VertexBuffers;
	}
	const Ref<CHEngine::IIndexBuffer>& VertexArrayOGL::GetIndexBuffer() const
	{
		return m_IndexBuffer;
	}
}