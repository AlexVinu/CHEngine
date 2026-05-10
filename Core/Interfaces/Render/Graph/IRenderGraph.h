#pragma once

#include "Render/Graph/PassDesc.h"
#include "Render/IFrameGraphBackend.h"

namespace CHEngine {

    /// Declarative render graph front-end.
    /// Callers add PassDesc objects; the graph topo-sorts and dispatches
    /// to the backend.
    class IRenderGraph {
    public:
        virtual ~IRenderGraph() = default;

        /// Clear accumulated passes for a new frame.
        virtual void Reset() = 0;

        /// Submit a pass for this frame.
        virtual void AddPass(PassDesc&& pass) = 0;

        /// Topological sort by reads/writes dependency edges.
        virtual void Compile() = 0;

        /// Dispatch sorted passes to the backend.
        virtual void Execute(IFrameGraphBackend& backend) = 0;
    };

} // namespace CHEngine
