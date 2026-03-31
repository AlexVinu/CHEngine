#include <chepch.h>
#include "RenderApiOGL.h"

#include <Log/Log.h>
#include <glad/glad.h>

namespace CHModules
{
	namespace {
		bool s_GLADInitialized = false;
	}

	RendererApiOGL::RendererApiOGL() = default;

	void RendererApiOGL::Init(const CHEngine::RendererInitInfo& init_info)
	{
		if (s_GLADInitialized)
			return;

		if (!init_info.OpenGL.Loader) {
			CHE_CORE_CRITICAL("RendererApiOGL::Init — GL loader function is NULL!");
			return;
		}

		int success = gladLoadGLLoader((GLADloadproc)init_info.OpenGL.Loader);
		if (!success) {
			CHE_CORE_CRITICAL("RendererApiOGL::Init - GLAD initialization failed (gladLoadGLLoader returned 0)!");
			return;
		}
		s_GLADInitialized = true;

		glEnable(GL_DEPTH_TEST);

		CHE_CORE_INFO("OpenGL initialized: {}", (const char*)glGetString(GL_VERSION));
	}

	void RendererApiOGL::Shutdown()
	{
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
}
