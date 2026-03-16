#pragma once

#include <Core.h>

#include "RenderData.h"

namespace CHEngine
{
    // Интерфейс OpenGL-рендерера.
    // Отвечает только за инициализацию GLAD и управление viewport.
    // Создание окна и управление контекстом вынесено в IWindow (WindowGLFW модуль).

    class IRenderer
    {
    public:
        virtual ~IRenderer() = default;

        // Инициализировать GLAD используя GL-контекст из существующего нативного окна
        virtual void Init(const RendererInitInfo& init_info) = 0;
        virtual void Shutdown() = 0;

        virtual void SetViewport(uint32_t width, uint32_t height) = 0;
    };
}
