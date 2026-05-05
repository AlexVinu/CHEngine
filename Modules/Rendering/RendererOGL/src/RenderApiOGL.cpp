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

		// Cache UBO offset alignment (typically 256 on most desktop GPUs).
		GLint align = 256;
		glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &align);
		m_UBOAlignment = static_cast<uint32_t>(align);

		CHE_CORE_INFO("OpenGL initialized: {} (UBO align={})",
		              (const char*)glGetString(GL_VERSION), m_UBOAlignment);
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

	void RendererApiOGL::DrawFullscreenTriangle()
	{
		// Lazily create a VAO + VBO holding 3 clip-space positions.
		// The covering triangle spans well beyond [-1,1]^2 so the rasterizer
		// clips it down to a fullscreen quad without needing a 4-vertex strip.
		if (m_FullscreenVAO == 0)
		{
			static const float kCoveringTri[6] = {
				-1.0f, -1.0f,
				 3.0f, -1.0f,
				-1.0f,  3.0f,
			};

			glGenVertexArrays(1, reinterpret_cast<GLuint*>(&m_FullscreenVAO));
			glGenBuffers     (1, reinterpret_cast<GLuint*>(&m_FullscreenVBO));

			glBindVertexArray(m_FullscreenVAO);
			glBindBuffer(GL_ARRAY_BUFFER, m_FullscreenVBO);
			glBufferData(GL_ARRAY_BUFFER, sizeof(kCoveringTri), kCoveringTri, GL_STATIC_DRAW);

			// Slot 0: float2 position. Matches `float2 Position : POSITION` in tonemap.slang.
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

			glBindBuffer(GL_ARRAY_BUFFER, 0);
			glBindVertexArray(0);
		}

		glBindVertexArray(m_FullscreenVAO);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		glBindVertexArray(0);
	}

	void RendererApiOGL::BindTextureSlot(uint32_t texID, uint32_t slot)
	{
		glActiveTexture(GL_TEXTURE0 + slot);
		glBindTexture(GL_TEXTURE_2D, texID);
	}
}
