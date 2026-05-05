#include "CommandListOGL.h"

#include <glad/glad.h>

namespace CHModules {

    void CommandListOGL::BindPipeline(CHEngine::PipelineHandle pipeline)
    {
        m_BoundPipeline = pipeline;
    }

    void CommandListOGL::BindVertexBuffer(CHEngine::BufferHandle, uint32_t)
    {
    }

    void CommandListOGL::BindIndexBuffer(CHEngine::BufferHandle)
    {
    }

    void CommandListOGL::BindTexture(CHEngine::TextureHandle, uint32_t)
    {
    }

    void CommandListOGL::BindUniformBuffer(CHEngine::BufferHandle, uint32_t)
    {
    }

    void CommandListOGL::SetViewport(int x, int y, int w, int h)
    {
        glViewport(x, y, w, h);
    }

    void CommandListOGL::SetScissor(int x, int y, int w, int h)
    {
        glScissor(x, y, w, h);
    }

    void CommandListOGL::Draw(uint32_t vertexCount, uint32_t firstVertex)
    {
        glDrawArrays(GL_TRIANGLES, static_cast<GLint>(firstVertex), static_cast<GLsizei>(vertexCount));
    }

    void CommandListOGL::DrawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t)
    {
        const auto offset = reinterpret_cast<const void*>(static_cast<uintptr_t>(firstIndex * sizeof(uint32_t)));
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount), GL_UNSIGNED_INT, offset);
    }

    void CommandListOGL::DrawFullscreenTriangle()
    {
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

} // namespace CHModules
