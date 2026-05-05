#pragma once

#include "Render/ICommandList.h"

namespace CHModules {

    class CommandListOGL final : public CHEngine::ICommandList
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

    private:
        CHEngine::PipelineHandle m_BoundPipeline{};
    };

} // namespace CHModules
