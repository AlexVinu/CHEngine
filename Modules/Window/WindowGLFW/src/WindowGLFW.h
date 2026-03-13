#pragma once

#include "Render/IWindow.h"

#include <GLFW/glfw3.h>

namespace CHModules {

    class WindowGLFW : public CHEngine::IWindow
    {
    public:
        WindowGLFW(uint32_t width, uint32_t height, const char* title,
                   CHEngine::ErrorCallbackFn errorCallbackFn);
        ~WindowGLFW() override;

        void Shutdown() override;

        void SwapBuffers() override;
        void PollEvents() override;

        void SetVSync(bool enabled) override;

        void SetWindowContext(const CHEngine::WindowContext& context) override;

        void* GetNativeWindow() override { return m_Window; }

        uint32_t GetWidth()  const override { return m_Width; }
        uint32_t GetHeight() const override { return m_Height; }

        static CHEngine::EventType ConvertFromGLFW(int action);

    private:
        GLFWwindow* m_Window = nullptr;
        CHEngine::WindowContext m_Context;
        uint32_t m_Width  = 0;
        uint32_t m_Height = 0;
    };

}
