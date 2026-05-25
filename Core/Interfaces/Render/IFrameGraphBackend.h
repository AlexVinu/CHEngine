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

        /// Called once after the frame graph is compiled (topologically sorted, validated),
        /// before the first Execute(). Backends may use this to pre-bake barriers, warm up
        /// pipelines, pre-allocate descriptor pools, or merge compatible passes.
        /// Default: no-op. OGL and Metal leave this unimplemented.
        virtual void OnGraphCompiled(const Vector<PassDesc>& passes) { (void)passes; }

        /// Execute all passes in the order provided.
        virtual void Execute(const Vector<PassDesc>& passes) = 0;
    };

} // namespace CHEngine
