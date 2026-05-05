#pragma once

#include <Core.h>
#include <CheStl/Vector.h>

#include "Render/Graph/PassDesc.h"

namespace CHEngine {

    /// Backend interface for the frame graph.
    /// Receives a pre-sorted list of PassDesc and translates them into
    /// native API commands (OGL draw calls, Vulkan command buffers, etc.).
    class IFrameGraphBackend {
    public:
        virtual ~IFrameGraphBackend() = default;

        /// Execute all passes in the order provided.
        virtual void Execute(const Vector<PassDesc>& passes) = 0;
    };

} // namespace CHEngine
