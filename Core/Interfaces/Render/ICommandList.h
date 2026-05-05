#pragma once

#include "Handles.h"

#include <cstdint>

namespace CHEngine {

    class ICommandList
    {
    public:
        virtual ~ICommandList() = default;

        virtual void BindPipeline(PipelineHandle pipeline) = 0;
        virtual void BindVertexBuffer(BufferHandle buffer, uint32_t slot) = 0;
        virtual void BindIndexBuffer(BufferHandle buffer) = 0;
        virtual void BindTexture(TextureHandle texture, uint32_t slot) = 0;
        virtual void BindUniformBuffer(BufferHandle buffer, uint32_t binding) = 0;
        virtual void SetViewport(int x, int y, int w, int h) = 0;
        virtual void SetScissor(int x, int y, int w, int h) = 0;
        virtual void Draw(uint32_t vertexCount, uint32_t firstVertex) = 0;
        virtual void DrawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t baseVertex) = 0;
        virtual void DrawFullscreenTriangle() = 0;
    };

} // namespace CHEngine
