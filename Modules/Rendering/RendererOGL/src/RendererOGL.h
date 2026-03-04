#pragma once

#include "Render/IRenderer.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
namespace CHModules
{
	class RendererOGL : public CHEngine::IRenderer
	{
	public:
		RendererOGL(const unsigned int width, const unsigned int height, const char* title, CHEngine::ErrorCallbackFn errorCallbackFn);
		virtual ~RendererOGL();

		virtual virtual void Init(const unsigned int width, const unsigned int height, const char* title, CHEngine::ErrorCallbackFn errorCallbackFn) override;
		virtual void Shutdown() override;

		virtual void SwapBuffers() override;
		virtual void PollEvents() override;

		virtual void SetVSync(bool enabled) override;

		virtual void SetViewport(uint32_t width, uint32_t height)  override;

		virtual void SetWindowContext(const CHEngine::RendererWindowContext& context) override;

		virtual void* GetNativeWindow() override { return m_Window; }

		static CHEngine::EventType ConvertFromGLFW(int action);
	private:
		CHEngine::RendererWindowContext m_WindowContext;
		GLFWwindow* m_Window;
	};
}