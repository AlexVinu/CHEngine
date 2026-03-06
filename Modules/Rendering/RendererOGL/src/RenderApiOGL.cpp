#include <chepch.h>
#include "RenderApiOGL.h"

#include<glad/glad.h>

namespace CHModules
{
	RendererApiOGL::RendererApiOGL()
	{
		glEnable(GL_DEPTH_TEST);   // enable depth testing once at creation
	}

	void RendererApiOGL::SetClearColor(float r, float g, float b, float a)
	{
		m_ClearR = r; m_ClearG = g; m_ClearB = b; m_ClearA = a;
	}

	void RendererApiOGL::Clear()
	{
		glClearColor(m_ClearR, m_ClearG, m_ClearB, m_ClearA);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	void RendererApiOGL::DrawIndexed(const CHEngine::IVertexArray* vertexArray)
	{
		vertexArray->Bind();
		glDrawElements(GL_TRIANGLES, vertexArray->GetIndexBuffer()->GetCount(), GL_UNSIGNED_INT, nullptr);
	}

	void RendererApiOGL::SetViewport(uint32_t width, uint32_t height)
	{
		glViewport(0, 0, width, height);
	}
}
