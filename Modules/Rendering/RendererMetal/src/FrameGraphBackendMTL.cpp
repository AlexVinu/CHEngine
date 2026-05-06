#include "FrameGraphBackendMTL.h"

#include <Log/Log.h>

namespace CHModules {

    void CommandListMTL::BindPipeline(CHEngine::PipelineHandle) {}
    void CommandListMTL::BindVertexBuffer(CHEngine::BufferHandle, uint32_t) {}
    void CommandListMTL::BindIndexBuffer(CHEngine::BufferHandle) {}
    void CommandListMTL::BindTexture(CHEngine::TextureHandle, uint32_t) {}
    void CommandListMTL::BindUniformBuffer(CHEngine::BufferHandle, uint32_t) {}
    void CommandListMTL::SetViewport(int, int, int, int) {}
    void CommandListMTL::SetScissor(int, int, int, int) {}
    void CommandListMTL::Draw(uint32_t, uint32_t) {}
    void CommandListMTL::DrawIndexed(uint32_t, uint32_t, int32_t) {}
    void CommandListMTL::DrawFullscreenTriangle() {}

    void FrameGraphBackendMTL::Execute(const CHEngine::Vector<CHEngine::PassDesc>& passes)
    {
        for (const CHEngine::PassDesc& pass : passes)
        {
            // TODO: Metal render pass setup (similar to OGL FBO setup)
            // - Create MTLRenderPassDescriptor from pass.ColorAttachments
            // - Attach textures via m_Factory.Textures.Get()
            // - Set viewport, clear state, etc.

            CHE_CORE_WARN("FrameGraphBackendMTL::Execute: Metal implementation pending");
        }
    }

} // namespace CHModules
