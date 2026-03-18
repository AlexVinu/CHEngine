#pragma once

#include "Render/IRenderer.h"

namespace CHEngine {
    class IImGuiLayer {
    public:
        virtual ~IImGuiLayer() = default;
        virtual void Begin() = 0;
        virtual void End() = 0;

        /// Передать нативный рендер-контекст (Metal: Device/CmdBuf/Encoder).
        /// OpenGL — не использует.
        virtual void SetRenderContext(const RenderContextInfo& /*ctx*/) {}
    };
}
