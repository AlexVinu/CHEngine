#pragma once

#include "Render/ICommandList.h"
#include "Render/IFrameGraphBackend.h"

namespace CHModules {

    class CommandListMTL final : public CHEngine::ICommandList
    {
    public:
        void BindPipeline(CHEngine::PipelineHandle pipeline) override;
        void BindVertexBuffer(CHEngine::BufferHandle buffer, uint32_t slot) override;
        void BindIndexBuffer(CHEngine::BufferHandle buffer) override;
        void BindTexture(CHEngine::TextureHandle texture, uint32_t slot) override;
        void BindUniformBuffer(CHEngine::BufferHandle buffer, uint32_t binding) override;
        void SetViewport(int x, int y, int w, int h) override;
        void SetScissor(int x, int y, int w, int h) override;
        void Draw(uint32_t vertexCount, uint32_t firstVertex) override;
        void DrawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t baseVertex) override;
        void DrawFullscreenTriangle() override;
    };

    class FrameGraphBackendMTL final : public CHEngine::IFrameGraphBackend
    {
    public:
        void Execute(const CHEngine::Vector<CHEngine::PassDesc>& passes) override;

    private:
        CommandListMTL m_CommandList;
    };

} // namespace CHModules
