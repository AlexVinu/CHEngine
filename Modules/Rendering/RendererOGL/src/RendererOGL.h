#pragma once

#include "Render/IRenderer.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace CHModules
{
	class RendererOGL : public CHEngine::IRenderer
	{
	public:
		RendererOGL(uint32_t width, uint32_t height);
		virtual ~RendererOGL();

		virtual void Init(uint32_t width, uint32_t height) override;
		virtual void Shutdown() override;

		virtual void SetViewport(uint32_t width, uint32_t height) override;
	};
}