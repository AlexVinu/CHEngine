#pragma once

#include "IVertexArray.h"

namespace CHEngine {

	class RendererAPI
	{
	public:
		virtual ~RendererAPI() = default;

		virtual void SetClearColor(float r, float g, float b, float a) = 0;
		virtual void Clear() = 0;

		virtual void DrawIndexed(const CHEngine::IVertexArray* vertexArray) = 0;
		virtual void DrawLines(const CHEngine::IVertexArray* vertexArray) = 0;
		virtual void SetViewport(uint32_t width, uint32_t height) = 0;
	};

}
