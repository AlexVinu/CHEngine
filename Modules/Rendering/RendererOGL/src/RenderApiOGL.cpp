#include <chepch.h>
#include "RenderApiOGL.h"

#include<glad/glad.h>

namespace CHModules
{
	void RendererApiOGL::Clear()
	{
		glClearColor(1, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT);
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