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

    void FrameGraphBackendMTL::BeginFrame()
    {
    }

    CHEngine::TextureHandle FrameGraphBackendMTL::MaterializeTransient(const CHEngine::RGTextureDesc& desc)
    {
        (void)desc;
        CHE_CORE_WARN("FrameGraphBackendMTL: transient texture materialization is pending Metal handle-pool migration");
        return CHEngine::TextureHandle::Invalid();
    }

    void FrameGraphBackendMTL::RecordPass(const CHEngine::CompiledPass&,
                                          CHEngine::IRGContext& context,
                                          const CHEngine::ExecuteFn& execute)
    {
        if (execute)
            execute(context, m_CommandList);
    }

    void FrameGraphBackendMTL::SubmitFrame()
    {
    }

} // namespace CHModules
