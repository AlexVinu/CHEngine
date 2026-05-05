#pragma once

#include "Render/Core/RenderAPI.h"

namespace CHEngine {
    class IImGuiLayer {
    public:
        virtual ~IImGuiLayer() = default;
        virtual void Begin() = 0;
        virtual void End() = 0;

        /// Pass native render context (Metal: Device/CmdBuf/Encoder).
        /// OpenGL — not used.
        virtual void SetRenderContext(const RenderContextInfo& /*ctx*/) {}
    };
}
