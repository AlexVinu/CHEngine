#include <chepch.h>
#include "RenderApiOGL.h"

#include <glad/glad.h>

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
		if (!vertexArray) return;
		vertexArray->Bind();
		const auto& ibo = vertexArray->GetIndexBuffer();
		if (!ibo) return;
		glDrawElements(GL_TRIANGLES, ibo->GetCount(), GL_UNSIGNED_INT, nullptr);
	}

	void RendererApiOGL::DrawLines(const CHEngine::IVertexArray* vertexArray)
	{
		if (!vertexArray) return;
		vertexArray->Bind();
		const auto& ibo = vertexArray->GetIndexBuffer();
		if (!ibo) return;
		glDrawElements(GL_LINES, ibo->GetCount(), GL_UNSIGNED_INT, nullptr);
	}

	void RendererApiOGL::SetViewport(uint32_t width, uint32_t height)
	{
		glViewport(0, 0, (GLsizei)width, (GLsizei)height);
	}

	void RendererApiOGL::SetBlend(bool enable)
	{
		if (enable) {
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		} else {
			glDisable(GL_BLEND);
		}
	}

	void RendererApiOGL::SetDepthWrite(bool enable)
	{
		glDepthMask(enable ? GL_TRUE : GL_FALSE);
	}
}
