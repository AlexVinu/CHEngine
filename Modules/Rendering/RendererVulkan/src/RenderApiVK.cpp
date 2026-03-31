#include "RenderApiVK.h"
#include "VulkanGlobals.h"

#include <Log/Log.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace CHModules
{
    void RenderApiVK::Init(const CHEngine::RendererInitInfo& init_info)
    {
        auto* window = static_cast<GLFWwindow*>(init_info.Vulkan.WindowHandle);
        if (!window) {
            CHE_CORE_CRITICAL("RenderApiVK::Init — WindowHandle is NULL!");
            return;
        }

        int w, h;
        glfwGetFramebufferSize(window, &w, &h);

        if (!m_Context.Init(window, static_cast<uint32_t>(w), static_cast<uint32_t>(h))) {
            CHE_CORE_CRITICAL("RenderApiVK::Init — VulkanContext init failed!");
            return;
        }

        VKGlobals::g_ContextPtr = &m_Context;
    }

    void RenderApiVK::Shutdown()
    {
        VKGlobals::g_ContextPtr = nullptr;
        m_Context.Shutdown();
    }

    bool RenderApiVK::BeginFrame()
    {
        return m_Context.BeginFrame();
    }

    void RenderApiVK::EndFrame()
    {
        m_Context.EndFrame();
    }

    void RenderApiVK::SetClearColor(float r, float g, float b, float a)
    {
        m_Context.SetClearColor(r, g, b, a);
    }

    void RenderApiVK::Clear()
    {
    }

    void RenderApiVK::SetViewport(uint32_t width, uint32_t height)
    {
        if (m_Context.GetDevice() != VK_NULL_HANDLE)
            m_Context.RecreateSwapchain(width, height);
    }

    void RenderApiVK::SetBlend(bool /*enable*/)
    {
    }

    void RenderApiVK::SetDepthWrite(bool /*enable*/)
    {
    }

    void RenderApiVK::DrawIndexed(const CHEngine::IVertexArray* /*vertexArray*/)
    {
    }

    void RenderApiVK::DrawLines(const CHEngine::IVertexArray* /*vertexArray*/)
    {
    }
}
