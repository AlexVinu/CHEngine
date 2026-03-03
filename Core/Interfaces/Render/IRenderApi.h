#pragma once

#include "IVertexArray.h"

namespace CHEngine {

	class RendererAPI
	{
	public:
		virtual ~RendererAPI() = default;

		virtual void Clear() = 0;

		virtual void DrawIndexed(const CHEngine::IVertexArray* vertexArray) = 0;
		virtual void SetViewport(uint32_t width, uint32_t height) = 0;

	};

}

