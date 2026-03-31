#pragma once

#include "Render/IRenderApi.h"

namespace CHModules {
	class RendererApiOGL : public CHEngine::IRenderApi
	{
	public:
		RendererApiOGL();
		~RendererApiOGL() override = default;

		void Init(const CHEngine::RendererInitInfo& init_info) override;
		void Shutdown() override;

		void SetClearColor(float r, float g, float b, float a) override;
		void Clear() override;

		void SetViewport(uint32_t width, uint32_t height) override;
		void SetBlend(bool enable) override;
		void SetDepthWrite(bool enable) override;

		void DrawIndexed(const CHEngine::IVertexArray* vertexArray) override;
		void DrawLines(const CHEngine::IVertexArray* vertexArray) override;

	private:
		float m_ClearR = 0.1f, m_ClearG = 0.1f, m_ClearB = 0.1f, m_ClearA = 1.0f;
	};
}
