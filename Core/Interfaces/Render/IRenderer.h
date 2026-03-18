#pragma once

#include <Core.h>

#include "RenderData.h"

namespace CHEngine
{
    /// Непрозрачный контекст для передачи нативных объектов рендерера
    /// в ImGui-бэкенд (Metal: MTLDevice, CommandBuffer, Encoder и т.д.).
    struct RenderContextInfo
    {
        void* Device               = nullptr;
        void* CommandBuffer        = nullptr;
        void* RenderEncoder        = nullptr;
        void* RenderPassDescriptor = nullptr;
    };

    class IRenderer
    {
    public:
        virtual ~IRenderer() = default;

        virtual void Init(const RendererInitInfo& init_info) = 0;
        virtual void Shutdown() = 0;

        virtual void SetViewport(uint32_t width, uint32_t height) = 0;

        /// Начать кадр (Metal: получить drawable, создать command buffer/encoder).
        /// OpenGL — no-op (контекст всегда готов).
        virtual bool BeginFrame() { return true; }

        /// Завершить кадр (Metal: end encoding, present, commit).
        /// OpenGL — no-op (SwapBuffers в Window).
        virtual void EndFrame() {}

        /// Нативные объекты текущего кадра для ImGui-бэкенда.
        virtual RenderContextInfo GetRenderContext() const { return {}; }
    };
}
