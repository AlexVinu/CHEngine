#include "VulkanContext.h"

#include <Log/Log.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <set>
#include <algorithm>
#include <limits>
#include <cstring>

#include "VKCore.h"

namespace CHModules {

    // ─── Validation layers ────────────────────────────────────────────────────
#ifdef CHE_DEBUG
    static const std::vector<const char*> s_ValidationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void*)
    {
        if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
            CHE_CORE_ERROR("Vulkan: {}", pCallbackData->pMessage);
        else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
            CHE_CORE_WARN("Vulkan: {}", pCallbackData->pMessage);
        else
            CHE_CORE_TRACE("Vulkan: {}", pCallbackData->pMessage);
        return VK_FALSE;
    }

    bool VulkanContext::CreateDebugMessenger()
    {
        VkDebugUtilsMessengerCreateInfoEXT createInfo = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType =
                VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = DebugCallback,
        };

        auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_Instance, "vkCreateDebugUtilsMessengerEXT"));
        if (func)
            return func(m_Instance, &createInfo, nullptr, &m_DebugMessenger) == VK_SUCCESS;
        return false;
    }

    void VulkanContext::DestroyDebugMessenger()
    {
        if (m_DebugMessenger == VK_NULL_HANDLE) return;
        auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_Instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (func) func(m_Instance, m_DebugMessenger, nullptr);
        m_DebugMessenger = VK_NULL_HANDLE;
    }
#endif

    // ─── Helpers ─────────────────────────────────────────────────────────────

    static VkFormat FindDepthFormat(VkPhysicalDevice device)
    {
        VkFormat candidates[] = {
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT
        };
        for (auto fmt : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(device, fmt, &props);
            if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
                return fmt;
        }
        return VK_FORMAT_D32_SFLOAT;
    }

    void VulkanContext::LoadDynamicRenderingFunctions()
    {
        if (m_InstanceVersion.Major > 1 || (m_InstanceVersion.Major == 1 && m_InstanceVersion.Minor >= 3)) {
            m_vkCmdBeginRendering = vkCmdBeginRendering;
            m_vkCmdEndRendering   = vkCmdEndRendering;
            CHE_CORE_INFO("Vulkan: dynamic rendering from core 1.3+");
            return;
        }

        m_vkCmdBeginRendering = reinterpret_cast<PFN_vkCmdBeginRendering>(
            vkGetDeviceProcAddr(m_Device, "vkCmdBeginRendering"));
        m_vkCmdEndRendering = reinterpret_cast<PFN_vkCmdEndRendering>(
            vkGetDeviceProcAddr(m_Device, "vkCmdEndRendering"));

        if (m_vkCmdBeginRendering && m_vkCmdEndRendering)
            CHE_CORE_INFO("Vulkan: loaded dynamic rendering via extension");
    }

    // ─── Init / Shutdown ──────────────────────────────────────────────────────

    bool VulkanContext::Init(GLFWwindow* window, uint32_t width, uint32_t height)
    {
        m_Window = window;
        if (!CreateInstance())       return false;
#ifdef CHE_DEBUG
        CreateDebugMessenger();
#endif
        if (!CreateSurface(window))  return false;
        if (!PickPhysicalDevice())   return false;
        if (!CreateLogicalDevice())  return false;

        LoadDynamicRenderingFunctions();

        if (!m_vkCmdBeginRendering || !m_vkCmdEndRendering) {
            CHE_CORE_CRITICAL("VulkanContext: dynamic rendering functions not available");
            return false;
        }

        if (!CreateAllocator())         return false;
        if (!CreateSwapchain(width, height)) return false;
        if (!CreateDepthResources())    return false;
        if (!CreateCommandPool())       return false;
        if (!CreateCommandBuffers())    return false;
        if (!CreateSyncObjects())       return false;
        if (!CreateDescriptorPools())   return false;

        CHE_CORE_INFO("Vulkan initialized successfully");
        return true;
    }

    void VulkanContext::Shutdown()
    {
        if (m_Device) vkDeviceWaitIdle(m_Device);

        // Reset all command buffers — breaks any recorded references to resources so that
        // destroying them below doesn't trigger "in use by recording command buffer" errors.
        if (m_CommandPool)
            vkResetCommandPool(m_Device, m_CommandPool, 0);

        // Flush all pending deferred deletions now that the GPU is idle.
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            m_DeletionQueues[i].clear();

        DestroyDescriptorPools();
        CleanupSwapchain();

        // RenderFinished semaphores are per-swapchain-image, so destroy all of them.
        for (auto sem : m_RenderFinishedSemaphores)
            if (sem) vkDestroySemaphore(m_Device, sem, nullptr);
        m_RenderFinishedSemaphores.clear();

        // ImageAvailable semaphores and fences are per-frame-in-flight.
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (i < m_ImageAvailableSemaphores.size() && m_ImageAvailableSemaphores[i])
                vkDestroySemaphore(m_Device, m_ImageAvailableSemaphores[i], nullptr);
            if (i < m_InFlightFences.size() && m_InFlightFences[i])
                vkDestroyFence(m_Device, m_InFlightFences[i], nullptr);
        }
        m_ImageAvailableSemaphores.clear();
        m_InFlightFences.clear();

        if (m_CommandPool) vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);

        if (m_Allocator) {
            vmaDestroyAllocator(m_Allocator);
            m_Allocator = VK_NULL_HANDLE;
        }

        if (m_Device) vkDestroyDevice(m_Device, nullptr);

#ifdef CHE_DEBUG
        DestroyDebugMessenger();
#endif
        if (m_Surface)  vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
        if (m_Instance) vkDestroyInstance(m_Instance, nullptr);
    }

    // ─── Per-frame ────────────────────────────────────────────────────────────

    bool VulkanContext::BeginFrame()
    {
        // Skip rendering while the window is minimized (framebuffer size = 0×0).
        if (m_Window) {
            int w = 0, h = 0;
            glfwGetFramebufferSize(m_Window, &w, &h);
            if (w == 0 || h == 0)
                return false;
        }

        vkWaitForFences(m_Device, 1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);

        VkResult result = vkAcquireNextImageKHR(m_Device, m_Swapchain, UINT64_MAX,
            m_ImageAvailableSemaphores[m_CurrentFrame], VK_NULL_HANDLE, &m_CurrentImageIdx);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            RecreateSwapchain(m_SwapchainExtent.width, m_SwapchainExtent.height);
            return false;
        }

        vkResetFences(m_Device, 1, &m_InFlightFences[m_CurrentFrame]);

        // GPU has finished with this frame's slot — safe to destroy deferred resources.
        m_DeletionQueues[m_CurrentFrame].clear();

        // Reset this frame's descriptor pool so sets from last use of this slot can be reused.
        vkResetDescriptorPool(m_Device, m_DescriptorPools[m_CurrentFrame], 0);

        VkCommandBuffer cmd = m_CommandBuffers[m_CurrentFrame];
        vkResetCommandBuffer(cmd, 0);

        VkCommandBufferBeginInfo beginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        vkBeginCommandBuffer(cmd, &beginInfo);

        // Transition swapchain image UNDEFINED → COLOR_ATTACHMENT_OPTIMAL.
        VkImageMemoryBarrier colorBarrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .image = m_SwapchainImages[m_CurrentImageIdx],
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0, .levelCount = 1,
                .baseArrayLayer = 0, .layerCount = 1,
            },
        };
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0, 0, nullptr, 0, nullptr, 1, &colorBarrier);

        // Transition depth image UNDEFINED → DEPTH_STENCIL_ATTACHMENT_OPTIMAL.
        VkImageMemoryBarrier depthBarrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .image = m_DepthImage,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .baseMipLevel = 0, .levelCount = 1,
                .baseArrayLayer = 0, .layerCount = 1,
            },
        };
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            0, 0, nullptr, 0, nullptr, 1, &depthBarrier);

        return true;
    }

    void VulkanContext::EndFrame()
    {
        VkCommandBuffer cmd = m_CommandBuffers[m_CurrentFrame];

        // Transition swapchain to PRESENT_SRC_KHR (from COLOR_ATTACHMENT_OPTIMAL, set by FG backend).
        VkImageMemoryBarrier presentBarrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = 0,
            .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .image = m_SwapchainImages[m_CurrentImageIdx],
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0, .levelCount = 1,
                .baseArrayLayer = 0, .layerCount = 1,
            },
        };
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0, 0, nullptr, 0, nullptr, 1, &presentBarrier);

        vkEndCommandBuffer(cmd);

        VkSemaphore waitSemaphores[]      = { m_ImageAvailableSemaphores[m_CurrentFrame] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        // Use per-swapchain-image semaphore so the presentation engine can hold it until
        // the image is re-acquired without conflicting with other in-flight frames.
        VkSemaphore signalSemaphores[] = { m_RenderFinishedSemaphores[m_CurrentImageIdx] };

        VkSubmitInfo submitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount   = 1,
            .pWaitSemaphores      = waitSemaphores,
            .pWaitDstStageMask    = waitStages,
            .commandBufferCount   = 1,
            .pCommandBuffers      = &cmd,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores    = signalSemaphores,
        };
        vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, m_InFlightFences[m_CurrentFrame]);

        VkPresentInfoKHR presentInfo = {
            .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores    = signalSemaphores,
            .swapchainCount     = 1,
            .pSwapchains        = &m_Swapchain,
            .pImageIndices      = &m_CurrentImageIdx,
        };
        VkResult result = vkQueuePresentKHR(m_PresentQueue, &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
            RecreateSwapchain(m_SwapchainExtent.width, m_SwapchainExtent.height);

        m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    // ─── Single-time commands ─────────────────────────────────────────────────

    VkCommandBuffer VulkanContext::BeginSingleTimeCommands()
    {
        VkCommandBufferAllocateInfo allocInfo = {
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool        = m_CommandPool,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        VkCommandBuffer cmd;
        vkAllocateCommandBuffers(m_Device, &allocInfo, &cmd);

        VkCommandBufferBeginInfo beginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        vkBeginCommandBuffer(cmd, &beginInfo);
        return cmd;
    }

    void VulkanContext::EndSingleTimeCommands(VkCommandBuffer cmd)
    {
        vkEndCommandBuffer(cmd);

        VkSubmitInfo submitInfo = {
            .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers    = &cmd,
        };
        vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(m_GraphicsQueue);
        vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &cmd);
    }

    // ─── Descriptor helpers ───────────────────────────────────────────────────

    VkDescriptorSet VulkanContext::AllocateDescriptorSet(VkDescriptorSetLayout layout)
    {
        VkDescriptorSetAllocateInfo allocInfo = {
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool     = m_DescriptorPools[m_CurrentFrame],
            .descriptorSetCount = 1,
            .pSetLayouts        = &layout,
        };
        VkDescriptorSet set = VK_NULL_HANDLE;
        VkResult res = vkAllocateDescriptorSets(m_Device, &allocInfo, &set);
        if (res != VK_SUCCESS)
        {
            CHE_CORE_ERROR("VulkanContext: failed to allocate descriptor set (pool full?)");
            return VK_NULL_HANDLE;
        }
        return set;
    }

    // ─── Deferred deletion queue ──────────────────────────────────────────────

    void VulkanContext::EnqueueDeletion(std::shared_ptr<void> resource)
    {
        m_DeletionQueues[m_CurrentFrame].push_back(std::move(resource));
    }

    // ─── Swapchain management ─────────────────────────────────────────────────

    void VulkanContext::RecreateSwapchain(uint32_t width, uint32_t height)
    {
        // Skip while minimized — BeginFrame already returns false so nothing is submitted.
        // The stale swapchain stays valid; when restored, BeginFrame resumes and triggers
        // recreation again with a non-zero extent.
        if (m_Window) {
            int fw = 0, fh = 0;
            glfwGetFramebufferSize(m_Window, &fw, &fh);
            if (fw == 0 || fh == 0)
                return;
        }

        vkDeviceWaitIdle(m_Device);

        // Destroy render-finished semaphores before the swapchain — they're per-image.
        DestroyRenderFinishedSemaphores();
        CleanupSwapchain();

        if (!CreateSwapchain(width, height)) {
            CHE_CORE_WARN("VulkanContext: swapchain recreation skipped (window minimized or 0×0 surface)");
            return;
        }
        if (!CreateDepthResources())
            CHE_CORE_ERROR("VulkanContext: failed to recreate depth resources");
        // Recreate render-finished semaphores for the new (possibly different) image count.
        if (!CreateRenderFinishedSemaphores())
            CHE_CORE_ERROR("VulkanContext: failed to recreate render-finished semaphores");
    }

    void VulkanContext::CleanupSwapchain()
    {
        if (m_DepthImageView) {
            vkDestroyImageView(m_Device, m_DepthImageView, nullptr);
            m_DepthImageView = VK_NULL_HANDLE;
        }
        if (m_DepthImage && m_DepthAlloc) {
            vmaDestroyImage(m_Allocator, m_DepthImage, m_DepthAlloc);
            m_DepthImage = VK_NULL_HANDLE;
            m_DepthAlloc = VK_NULL_HANDLE;
        }

        for (auto iv : m_SwapchainImageViews)
            vkDestroyImageView(m_Device, iv, nullptr);
        m_SwapchainImageViews.clear();

        if (m_Swapchain) {
            vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
            m_Swapchain = VK_NULL_HANDLE;
        }
    }

    // ─── Create functions ─────────────────────────────────────────────────────

    bool VulkanContext::CreateInstance()
    {
        GetInstanceVersion();

        VkApplicationInfo appInfo = {
            .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName   = "CHEngine",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName        = "CHEngine",
            .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion         = VK_MAKE_API_VERSION(0, m_InstanceVersion.Major, m_InstanceVersion.Minor, 0),
        };

        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

#ifdef CHE_DEBUG
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
#ifdef CHE_PLATFORM_APPLE
        extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif

        VkInstanceCreateInfo createInfo = {
            .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo        = &appInfo,
            .enabledExtensionCount   = static_cast<uint32_t>(extensions.size()),
            .ppEnabledExtensionNames = extensions.data(),
        };
#ifdef CHE_PLATFORM_APPLE
        createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif
#ifdef CHE_DEBUG
        createInfo.enabledLayerCount   = static_cast<uint32_t>(s_ValidationLayers.size());
        createInfo.ppEnabledLayerNames = s_ValidationLayers.data();
#else
        createInfo.enabledLayerCount = 0;
#endif
        VK_CHECK_RESULT(vkCreateInstance(&createInfo, nullptr, &m_Instance), "Create instance");
        return true;
    }

    bool VulkanContext::CreateSurface(GLFWwindow* window)
    {
        VK_CHECK_RESULT(glfwCreateWindowSurface(m_Instance, window, nullptr, &m_Surface), "Create surface");
        return true;
    }

    bool VulkanContext::PickPhysicalDevice()
    {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            CHE_CORE_CRITICAL("VulkanContext: no Vulkan-capable GPU found");
            return false;
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

        for (auto device : devices) {
            uint32_t queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

            bool foundGraphics = false, foundPresent = false;
            for (uint32_t i = 0; i < queueFamilyCount; i++) {
                if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                    m_GraphicsQueueFamily = i;
                    foundGraphics = true;
                }
                VkBool32 presentSupport = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface, &presentSupport);
                if (presentSupport) {
                    m_PresentQueueFamily = i;
                    foundPresent = true;
                }
                if (foundGraphics && foundPresent) break;
            }

            if (!foundGraphics || !foundPresent) continue;

            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(device, &props);

            const bool isVulkan13 = props.apiVersion >= VK_API_VERSION_1_3;
            if (!isVulkan13) {
                uint32_t extCount = 0;
                vkEnumerateDeviceExtensionProperties(device, nullptr, &extCount, nullptr);
                std::vector<VkExtensionProperties> exts(extCount);
                vkEnumerateDeviceExtensionProperties(device, nullptr, &extCount, exts.data());

                bool hasDynRendering = false;
                for (const auto& ext : exts)
                    if (std::strcmp(ext.extensionName, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME) == 0)
                        { hasDynRendering = true; break; }

                if (!hasDynRendering) {
                    CHE_CORE_WARN("Vulkan GPU {} doesn't support dynamic rendering, skipping",
                        props.deviceName);
                    continue;
                }
            }

            m_PhysicalDevice = device;
            CHE_CORE_INFO("Vulkan GPU: {} (API {}.{}.{})",
                props.deviceName,
                VK_API_VERSION_MAJOR(props.apiVersion),
                VK_API_VERSION_MINOR(props.apiVersion),
                VK_API_VERSION_PATCH(props.apiVersion));
            return true;
        }

        CHE_CORE_CRITICAL("VulkanContext: no suitable GPU with dynamic rendering support found");
        return false;
    }

    bool VulkanContext::CreateLogicalDevice()
    {
        std::set<uint32_t> uniqueQueueFamilies = { m_GraphicsQueueFamily, m_PresentQueueFamily };

        float queuePriority = 1.0f;
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        for (uint32_t qf : uniqueQueueFamilies) {
            queueCreateInfos.push_back({
                .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = qf,
                .queueCount       = 1,
                .pQueuePriorities = &queuePriority,
            });
        }

        std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

        if (m_InstanceVersion.Major < 1 || (m_InstanceVersion.Major == 1 && m_InstanceVersion.Minor < 3))
            deviceExtensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);

#ifdef CHE_PLATFORM_APPLE
        deviceExtensions.push_back("VK_KHR_portability_subset");
#endif

        VkPhysicalDeviceDynamicRenderingFeatures dynRenderingFeatures = {
            .sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
            .dynamicRendering = VK_TRUE,
        };

        // Enable descriptorBindingPartiallyBound for flexible descriptor sets.
        VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
            .pNext = &dynRenderingFeatures,
            .descriptorBindingPartiallyBound = VK_TRUE,
        };

        // shaderDrawParameters: required by Slang SPIR-V (DrawIndex/BaseVertex builtins).
        // Became core in Vulkan 1.1 — GTX 1650 Ti supports 1.4, so it's always present.
        VkPhysicalDeviceVulkan11Features vk11Features = {
            .sType                = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
            .pNext                = &indexingFeatures,
            .shaderDrawParameters = VK_TRUE,
        };

        VkPhysicalDeviceFeatures2 deviceFeatures2 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = &vk11Features,
        };
        // Enable anisotropic filtering.
        deviceFeatures2.features.samplerAnisotropy = VK_TRUE;

        VkDeviceCreateInfo createInfo = {
            .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext                   = &deviceFeatures2,
            .queueCreateInfoCount    = static_cast<uint32_t>(queueCreateInfos.size()),
            .pQueueCreateInfos       = queueCreateInfos.data(),
            .enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size()),
            .ppEnabledExtensionNames = deviceExtensions.data(),
        };

        VK_CHECK_RESULT(vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device), "Create device");
        vkGetDeviceQueue(m_Device, m_GraphicsQueueFamily, 0, &m_GraphicsQueue);
        vkGetDeviceQueue(m_Device, m_PresentQueueFamily,  0, &m_PresentQueue);
        return true;
    }

    bool VulkanContext::CreateAllocator()
    {
        VmaAllocatorCreateInfo allocatorInfo = {};
        allocatorInfo.physicalDevice = m_PhysicalDevice;
        allocatorInfo.device         = m_Device;
        allocatorInfo.instance       = m_Instance;
        // VMA 3.1.0 supports up to Vulkan 1.3; clamp so we don't trigger its version assertion.
        uint32_t vmaMinor = std::min(m_InstanceVersion.Minor, 3);
        allocatorInfo.vulkanApiVersion = VK_MAKE_API_VERSION(0, m_InstanceVersion.Major, vmaMinor, 0);

        VK_CHECK_RESULT(vmaCreateAllocator(&allocatorInfo, &m_Allocator), "Create VMA allocator");
        return true;
    }

    bool VulkanContext::CreateSwapchain(uint32_t width, uint32_t height)
    {
        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevice, m_Surface, &capabilities);

        uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &formatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &formatCount, formats.data());

        // Predпочитаем UNORM, а не SRGB swapchain: tonemap-проход уже сам пишет
        // gamma-corrected пиксели; SRGB-формат заставлял бы GPU применять
        // linear→sRGB ещё раз, что давало двойную коррекцию (UI тускнел, ImGui
        // выглядел вылинявшим). С UNORM и tonemap, и ImGui (рисующий в sRGB-space
        // напрямую) попадают в swapchain без искажений.
        m_SwapchainFormat = formats[0].format;
        VkColorSpaceKHR colorSpace = formats[0].colorSpace;
        for (const auto& fmt : formats) {
            if (fmt.format == VK_FORMAT_B8G8R8A8_UNORM && fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                m_SwapchainFormat = fmt.format;
                colorSpace = fmt.colorSpace;
                break;
            }
        }

        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            m_SwapchainExtent = capabilities.currentExtent;
        } else {
            m_SwapchainExtent.width  = std::clamp(width,  capabilities.minImageExtent.width,  capabilities.maxImageExtent.width);
            m_SwapchainExtent.height = std::clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        }

        // Surface reports 0×0 while the window is minimized — bail out, caller retries next frame.
        if (m_SwapchainExtent.width == 0 || m_SwapchainExtent.height == 0)
            return false;

        uint32_t imageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
            imageCount = capabilities.maxImageCount;

        VkSwapchainCreateInfoKHR swapInfo = {
            .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface          = m_Surface,
            .minImageCount    = imageCount,
            .imageFormat      = m_SwapchainFormat,
            .imageColorSpace  = colorSpace,
            .imageExtent      = m_SwapchainExtent,
            .imageArrayLayers = 1,
            .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .preTransform     = capabilities.currentTransform,
            .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode      = VK_PRESENT_MODE_FIFO_KHR,
            .clipped          = VK_TRUE,
        };

        uint32_t queueFamilyIndices[] = { m_GraphicsQueueFamily, m_PresentQueueFamily };
        if (m_GraphicsQueueFamily != m_PresentQueueFamily) {
            swapInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
            swapInfo.queueFamilyIndexCount = 2;
            swapInfo.pQueueFamilyIndices   = queueFamilyIndices;
        } else {
            swapInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        VK_CHECK_RESULT(vkCreateSwapchainKHR(m_Device, &swapInfo, nullptr, &m_Swapchain), "Create swapchain");

        vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, nullptr);
        m_SwapchainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, m_SwapchainImages.data());

        m_SwapchainImageViews.resize(imageCount);
        for (uint32_t i = 0; i < imageCount; i++) {
            VkImageViewCreateInfo viewInfo = {
                .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image    = m_SwapchainImages[i],
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format   = m_SwapchainFormat,
                .subresourceRange = {
                    .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel   = 0, .levelCount   = 1,
                    .baseArrayLayer = 0, .layerCount   = 1,
                },
            };
            VK_CHECK_RESULT(vkCreateImageView(m_Device, &viewInfo, nullptr, &m_SwapchainImageViews[i]), "Create swapchain image view");
        }

        return true;
    }

    bool VulkanContext::CreateDepthResources()
    {
        m_DepthFormat = FindDepthFormat(m_PhysicalDevice);

        VkImageCreateInfo imageInfo = {
            .sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType   = VK_IMAGE_TYPE_2D,
            .format      = m_DepthFormat,
            .extent      = { m_SwapchainExtent.width, m_SwapchainExtent.height, 1 },
            .mipLevels   = 1,
            .arrayLayers = 1,
            .samples     = VK_SAMPLE_COUNT_1_BIT,
            .tiling      = VK_IMAGE_TILING_OPTIMAL,
            .usage       = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VK_CHECK_RESULT(vmaCreateImage(m_Allocator, &imageInfo, &allocInfo, &m_DepthImage, &m_DepthAlloc, nullptr),
            "Create depth image");

        // Determine aspect mask based on format.
        VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (m_DepthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT || m_DepthFormat == VK_FORMAT_D24_UNORM_S8_UINT)
            aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

        VkImageViewCreateInfo viewInfo = {
            .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image    = m_DepthImage,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format   = m_DepthFormat,
            .subresourceRange = {
                .aspectMask     = aspectMask,
                .baseMipLevel   = 0, .levelCount   = 1,
                .baseArrayLayer = 0, .layerCount   = 1,
            },
        };
        VK_CHECK_RESULT(vkCreateImageView(m_Device, &viewInfo, nullptr, &m_DepthImageView), "Create depth image view");
        return true;
    }

    bool VulkanContext::CreateCommandPool()
    {
        VkCommandPoolCreateInfo poolInfo = {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = m_GraphicsQueueFamily,
        };
        VK_CHECK_RESULT(vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_CommandPool), "Create command pool");
        return true;
    }

    bool VulkanContext::CreateCommandBuffers()
    {
        m_CommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        VkCommandBufferAllocateInfo allocInfo = {
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool        = m_CommandPool,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = MAX_FRAMES_IN_FLIGHT,
        };
        VK_CHECK_RESULT(vkAllocateCommandBuffers(m_Device, &allocInfo, m_CommandBuffers.data()), "Allocate command buffers");
        return true;
    }

    bool VulkanContext::CreateSyncObjects()
    {
        // ImageAvailable semaphores and InFlightFences: one per frame-in-flight slot.
        m_ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semInfo = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        VkFenceCreateInfo fenceInfo   = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT };

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VK_CHECK_RESULT(vkCreateSemaphore(m_Device, &semInfo,  nullptr, &m_ImageAvailableSemaphores[i]), "Create imageAvailable semaphore");
            VK_CHECK_RESULT(vkCreateFence(m_Device,     &fenceInfo, nullptr, &m_InFlightFences[i]),          "Create in-flight fence");
        }

        // RenderFinished semaphores: one per swapchain image.
        // The presentation engine holds the semaphore until the swapchain image is re-acquired,
        // so we MUST index by image index, not by frame-in-flight slot. Otherwise the same
        // semaphore may be reused while the presentation engine still owns it for a different image.
        return CreateRenderFinishedSemaphores();
    }

    bool VulkanContext::CreateRenderFinishedSemaphores()
    {
        const uint32_t imageCount = static_cast<uint32_t>(m_SwapchainImages.size());
        m_RenderFinishedSemaphores.resize(imageCount);

        VkSemaphoreCreateInfo semInfo = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        for (uint32_t i = 0; i < imageCount; i++)
            VK_CHECK_RESULT(vkCreateSemaphore(m_Device, &semInfo, nullptr, &m_RenderFinishedSemaphores[i]),
                            "Create renderFinished semaphore");
        return true;
    }

    void VulkanContext::DestroyRenderFinishedSemaphores()
    {
        for (auto sem : m_RenderFinishedSemaphores)
            if (sem) vkDestroySemaphore(m_Device, sem, nullptr);
        m_RenderFinishedSemaphores.clear();
    }

    bool VulkanContext::CreateDescriptorPools()
    {
        // Large pool supporting many UBOs and combined image samplers.
        VkDescriptorPoolSize poolSizes[] = {
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         MAX_DESCRIPTORS_PER_TYPE },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_DESCRIPTORS_PER_TYPE },
        };

        VkDescriptorPoolCreateInfo poolInfo = {
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .flags         = 0, // reset entire pool per frame — no need to free individual sets
            .maxSets       = MAX_DESCRIPTOR_SETS_PER_FRAME,
            .poolSizeCount = 2,
            .pPoolSizes    = poolSizes,
        };

        m_DescriptorPools.resize(MAX_FRAMES_IN_FLIGHT);
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            VK_CHECK_RESULT(vkCreateDescriptorPool(m_Device, &poolInfo, nullptr, &m_DescriptorPools[i]), "Create descriptor pool");

        return true;
    }

    void VulkanContext::DestroyDescriptorPools()
    {
        for (auto pool : m_DescriptorPools)
            if (pool) vkDestroyDescriptorPool(m_Device, pool, nullptr);
        m_DescriptorPools.clear();
    }

    void VulkanContext::GetInstanceVersion()
    {
        uint32_t version = 0;
        VK_CHECK_RESULT(vkEnumerateInstanceVersion(&version), "Enumerate instance version");
        m_InstanceVersion = {
            .Major = static_cast<int>(VK_API_VERSION_MAJOR(version)),
            .Minor = static_cast<int>(VK_API_VERSION_MINOR(version)),
            .Patch = static_cast<int>(VK_API_VERSION_PATCH(version)),
        };
        CHE_CORE_INFO("Vulkan Instance Version: {}.{}.{}",
            m_InstanceVersion.Major, m_InstanceVersion.Minor, m_InstanceVersion.Patch);
    }

} // namespace CHModules
