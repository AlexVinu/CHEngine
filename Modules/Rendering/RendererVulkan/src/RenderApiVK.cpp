#include "RenderApiVK.h"

#include <Log/Log.h>

namespace CHModules
{
    void RenderApiVK::SetClearColor(float r, float g, float b, float a)
    {
        m_ClearR = r; m_ClearG = g; m_ClearB = b; m_ClearA = a;
    }

    void RenderApiVK::Clear()
    {
        // Clear выполняется в BeginFrame VulkanContext через VkClearValue
    }

    void RenderApiVK::DrawIndexed(const CHEngine::IVertexArray* /*vertexArray*/)
    {
        // TODO: Vulkan draw indexed — требует pipeline, descriptor sets
        // Скелетная реализация — будет расширена при добавлении pipeline
    }

    void RenderApiVK::DrawLines(const CHEngine::IVertexArray* /*vertexArray*/)
    {
        // TODO: Vulkan draw lines
    }

    void RenderApiVK::SetViewport(uint32_t /*width*/, uint32_t /*height*/)
    {
        // Viewport устанавливается в VulkanContext::BeginFrame
    }

    void RenderApiVK::SetBlend(bool /*enable*/)
    {
        // Blending настраивается в pipeline state
    }

    void RenderApiVK::SetDepthWrite(bool /*enable*/)
    {
        // Depth write настраивается в pipeline state
    }
}
