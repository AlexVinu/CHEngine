#pragma once

#include "Render/IRenderApi.h"

namespace CHModules {
	class RendererApiOGL : public CHEngine::RendererAPI
	{

		virtual ~RendererApiOGL() = default;

		virtual void Clear() override;

		virtual void DrawIndexed(const CHEngine::IVertexArray* vertexArray) override;
		virtual void SetViewport(uint32_t width, uint32_t height) override;
	};
}