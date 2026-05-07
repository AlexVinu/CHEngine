#pragma once

#include "Render/IFrameGraphBackend.h"

namespace CHModules {

    struct RenderFactoryMTL;

    /// Metal implementation of IFrameGraphBackend.
    /// Translates a sorted list of PassDesc into Metal render commands.
    ///
    /// Per pass:
    ///   1. Build MTLRenderPassDescriptor from ColorAttachments / DepthAttachment
    ///   2. Open a new render command encoder
    ///   3. Set viewport, pipeline state, depth stencil state
    ///   4. Bind pass-level uniforms and textures
    ///   5. Execute draw calls (or fullscreen triangle)
    ///   6. End the encoder
    ///
    /// After all framegraph passes, the screen encoder is restored so ImGui
    /// can render on top.
    class FrameGraphBackendMTL final : public CHEngine::IFrameGraphBackend
    {
    public:
        explicit FrameGraphBackendMTL(RenderFactoryMTL& factory);

        void Execute(const CHEngine::Vector<CHEngine::PassDesc>& passes) override;

    private:
        RenderFactoryMTL& m_Factory;
    };

} // namespace CHModules
