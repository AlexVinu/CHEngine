#pragma once

#include "WindowHandler/IWindowHandler.h"

#include <GLFW/glfw3.h>
namespace CHModules
{
	class WindowHandlerGLFW : public CHEngine::IWindowHandler
	{
	public:
		WindowHandlerGLFW(const unsigned int width, const unsigned int height, const char* title, CHEngine::ErrorCallbackFn errorCallbackFn);
		virtual ~WindowHandlerGLFW();

		virtual virtual void Init(const unsigned int width, const unsigned int height, const char* title, CHEngine::ErrorCallbackFn errorCallbackFn) override;
		virtual void Shutdown() override;

		virtual void SwapBuffers() override;
		virtual void PollEvents() override;

		virtual void SetVSync(bool enabled) override;

		//virtual void SetViewport(uint32_t width, uint32_t height, (void*)(uint32_t, uint32_t))  override;

		virtual void SetWindowContext(const CHEngine::RendererWindowContext& context) override;

		virtual void* GetNativeWindow() override { return m_Window; }

		static CHEngine::EventType ConvertFromGLFW(int action);
	private:
		CHEngine::RendererWindowContext m_WindowContext;
		GLFWwindow* m_Window;
	};
}