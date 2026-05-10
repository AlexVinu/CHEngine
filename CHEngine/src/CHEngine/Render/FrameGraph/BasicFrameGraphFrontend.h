#pragma once

#include <Render/Graph/IRenderGraph.h>
#include <CheStl/Vector.h>

namespace CHEngine {

    /// Minimal frame-graph frontend.
    /// Accumulates PassDesc objects per frame, performs Kahn's-algorithm
    /// topological sort in Compile(), then dispatches to the backend in Execute().
    class BasicFrameGraphFrontend final : public IRenderGraph
    {
    public:
        BasicFrameGraphFrontend() = default;
        ~BasicFrameGraphFrontend() override = default;

        void Reset()                          override;
        void AddPass(PassDesc&& pass)         override;
        void Compile()                        override;
        void Execute(IFrameGraphBackend& backend) override;

    private:
        Vector<PassDesc> m_Passes;       // accumulated this frame
        Vector<PassDesc> m_Sorted;       // result of Compile()
    };

} // namespace CHEngine
