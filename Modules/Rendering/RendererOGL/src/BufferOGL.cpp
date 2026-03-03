#include <chepch.h>
#include "BufferOGL.h"

#include<glad/glad.h>

namespace CHModules
{
	VertexBufferOGL::VertexBufferOGL(float* verticies, uint32_t size)
	{
		glCreateBuffers(1, &m_RendererID);
		glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
		glBufferData(GL_ARRAY_BUFFER, size, verticies, GL_STATIC_DRAW);

	}
	VertexBufferOGL::~VertexBufferOGL()
	{
		glDeleteBuffers(1, &m_RendererID);
	}

	void VertexBufferOGL::Bind() const
	{
		glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
	}
	void VertexBufferOGL::Unbind() const
	{
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	const CHEngine::BufferLayout& VertexBufferOGL::GetLayout() const
	{
		return m_Layout;
	}

	void VertexBufferOGL::SetLayout(const CHEngine::BufferLayout& layout)
	{
		m_Layout = layout;
	}
	// Index buffer

	IndexBufferOGL::IndexBufferOGL(uint32_t* indices, uint32_t count)
		:m_Count(count)
	{
		glCreateBuffers(1, &m_RendererID);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(uint32_t), indices, GL_STATIC_DRAW);

	}
	IndexBufferOGL::~IndexBufferOGL()
	{
		glDeleteBuffers(1, &m_RendererID);
	}

	void IndexBufferOGL::Bind() const
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
	}
	void IndexBufferOGL::Unbind() const
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}

	uint32_t IndexBufferOGL::GetCount() const
	{
		return m_Count;
	}
}

