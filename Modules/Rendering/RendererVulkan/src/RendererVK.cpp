#include "RendererVK.h"

#include <Log/Log.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace CHModules
{
    void RendererVK::Init(const CHEngine::RendererInitInfo& init_info)
    {
        auto* window = static_cast<GLFWwindow*>(init_info.Vulkan.WindowHandle);
        if (!window) {
            CHE_CORE_CRITICAL("RendererVK::Init — WindowHandle is NULL!");
            return;
        }

        int w, h;
        glfwGetFramebufferSize(window, &w, &h);

        if (!m_Context.Init(window, static_cast<uint32_t>(w), static_cast<uint32_t>(h))) {
            CHE_CORE_CRITICAL("RendererVK::Init — VulkanContext init failed!");
        }
    }

    void RendererVK::Shutdown()
    {
        m_Context.Shutdown();
    }

    void RendererVK::SetViewport(uint32_t width, uint32_t height)
    {
        m_Context.RecreateSwapchain(width, height);
    }
}
