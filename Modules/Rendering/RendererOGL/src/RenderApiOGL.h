#pragma once

#include "Render/IRenderApi.h"

namespace CHModules {
	class RendererApiOGL : public CHEngine::RendererAPI
	{
	public:
		RendererApiOGL();
		virtual ~RendererApiOGL() = default;

		virtual void SetClearColor(float r, float g, float b, float a) override;
		virtual void Clear() override;

		virtual void DrawIndexed(const CHEngine::IVertexArray* vertexArray) override;
		virtual void DrawLines(const CHEngine::IVertexArray* vertexArray) override;
		virtual void SetViewport(uint32_t width, uint32_t height) override;

	private:
		float m_ClearR = 0.1f, m_ClearG = 0.1f, m_ClearB = 0.1f, m_ClearA = 1.0f;
	};
}
