#pragma once

#include "Render/IRenderer.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace CHModules
{
    // OpenGL рендерер. Отвечает только за инициализацию GLAD и управление viewport.
    // Создание окна, swap buffers, события — вынесены в WindowGLFW модуль.
    class RendererOGL : public CHEngine::IRenderer
    {
    public:
        RendererOGL() = default;
        ~RendererOGL() override = default;

        // Инициализировать GLAD используя текущий GL-контекст (из WindowGLFW).
        // nativeWindow используется только для получения glfwGetProcAddress.
        void Init(void* nativeWindow) override;
        void Shutdown() override {}

        void SetViewport(uint32_t width, uint32_t height) override;
    };
}
