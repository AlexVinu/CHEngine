#pragma once

#include "Render/Descriptors.h"

namespace CHModules {

    struct RenderFactoryMTL;

    /// Metal pipeline state object built from PipelineDesc.
    /// Wraps MTLRenderPipelineState + MTLDepthStencilState.
    class PipelineMTL {
    public:
        explicit PipelineMTL(const CHEngine::PipelineDesc& desc,
                             RenderFactoryMTL& factory);
        ~PipelineMTL();

        PipelineMTL(const PipelineMTL&)            = delete;
        PipelineMTL& operator=(const PipelineMTL&) = delete;

        void* GetPSO()        const { return m_PSO; }         // id<MTLRenderPipelineState>
        void* GetDepthState() const { return m_DepthState; }  // id<MTLDepthStencilState>

        const CHEngine::PipelineDesc& GetDesc() const { return m_Desc; }

    private:
        static uint32_t ToMTLPixelFormat(CHEngine::TextureFormat fmt);
        static uint32_t ToMTLCompareFunc(CHEngine::CompareOp op);
        static uint32_t ToMTLBlendFactor(CHEngine::BlendFactor f);
        static uint32_t ToMTLBlendOp(CHEngine::BlendOp op);

        CHEngine::PipelineDesc m_Desc;
        void* m_PSO        = nullptr; // id<MTLRenderPipelineState>
        void* m_DepthState = nullptr; // id<MTLDepthStencilState>
    };

} // namespace CHModules
