#pragma once

#include "WindowSystem/IWindow.h"

#include <GLFW/glfw3.h>

namespace CHModules {

    class WindowGLFW : public CHEngine::IWindow
    {
    public:
        WindowGLFW(uint32_t width, uint32_t height, const char* title,
                   CHEngine::ErrorCallbackFn errorCallbackFn, CHEngine::ERenderAPI renderApi);
        ~WindowGLFW() override;

        void Shutdown() override;

        void SwapBuffers() override;
        void PollEvents() override;

        void SetVSync(bool enabled) override;

		virtual void SetMouse(bool) override;

        void SetWindowContext(const CHEngine::WindowContext& context) override;

        void* GetNativeWindow() override { return m_Window; }

        uint32_t GetWidth()  const override { return m_Width; }
        uint32_t GetHeight() const override { return m_Height; }

        virtual CHEngine::RendererInitInfo GetRenderInitInfo(CHEngine::ERenderAPI render_api) const override;
        bool IsKeyDown(int key) const override;
        bool IsMouseButtonDown(int button) const override;
        void GetMousePosition(float& x, float& y) const override;

        void  GetWindowPos(int& x, int& y) const override;
        void  SetWindowPos(int x, int y) override;
        void  SetWindowSize(uint32_t w, uint32_t h) override;
        float GetScrollDelta() const override  { return m_ScrollDelta; }
        void  ClearScrollDelta()       override { m_ScrollDelta = 0.0f; }

    private:
        GLFWwindow* m_Window = nullptr;
        CHEngine::WindowContext m_Context;
        CHEngine::ERenderAPI m_RenderAPI = CHEngine::ERenderAPI::NONE;
        uint32_t m_Width  = 0;
        uint32_t m_Height = 0;
        float    m_ScrollDelta = 0.0f;
    };

}
