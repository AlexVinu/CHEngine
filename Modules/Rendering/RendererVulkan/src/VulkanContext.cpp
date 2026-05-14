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
		VkDebugUtilsMessageTypeFlagsEXT /*type*/,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* /*pUserData*/)
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

		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)
			vkGetInstanceProcAddr(m_Instance, "vkCreateDebugUtilsMessengerEXT");
		if (func)
			return func(m_Instance, &createInfo, nullptr, &m_DebugMessenger) == VK_SUCCESS;
		return false;
	}

	void VulkanContext::DestroyDebugMessenger()
	{
		if (m_DebugMessenger == VK_NULL_HANDLE) return;
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)
			vkGetInstanceProcAddr(m_Instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func) func(m_Instance, m_DebugMessenger, nullptr);
		m_DebugMessenger = VK_NULL_HANDLE;
	}
#endif

	// ─── Helper: найти подходящий формат depth ─────────────────────────────
	static VkFormat FindDepthFormat(VkPhysicalDevice device)
	{
		VkFormat candidates[] = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
		for (auto fmt : candidates) {
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(device, fmt, &props);
			if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
				return fmt;
		}
		return VK_FORMAT_D32_SFLOAT;
	}

	static uint32_t FindMemoryType(VkPhysicalDevice physDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties)
	{
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(physDevice, &memProperties);
		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
			if ((typeFilter & (1 << i)) &&
				(memProperties.memoryTypes[i].propertyFlags & properties) == properties)
				return i;
		}
		return 0;
	}

	// ─── Загрузка функций dynamic rendering ─────────────────────────────────
	void VulkanContext::LoadDynamicRenderingFunctions()
	{
		// Если Vulkan 1.3+, функции доступны напрямую через статическое связывание
		if (m_InstanceVersion.Major > 1 || (m_InstanceVersion.Major == 1 && m_InstanceVersion.Minor >= 3)) {
			m_vkCmdBeginRendering = vkCmdBeginRendering;
			m_vkCmdEndRendering = vkCmdEndRendering;
			CHE_CORE_INFO("Vulkan: using dynamic rendering from core API 1.3+");
			return;
		}

		// Для Vulkan 1.2 и ниже загружаем через vkGetDeviceProcAddr
		m_vkCmdBeginRendering = reinterpret_cast<PFN_vkCmdBeginRendering>(
			vkGetDeviceProcAddr(m_Device, "vkCmdBeginRendering"));
		m_vkCmdEndRendering = reinterpret_cast<PFN_vkCmdEndRendering>(
			vkGetDeviceProcAddr(m_Device, "vkCmdEndRendering"));

		if (m_vkCmdBeginRendering && m_vkCmdEndRendering) {
			CHE_CORE_INFO("Vulkan: loaded dynamic rendering functions via extension");
		}
	}

	// ─── Init ─────────────────────────────────────────────────────────────────
	bool VulkanContext::Init(GLFWwindow* window, uint32_t width, uint32_t height)
	{
		if (!CreateInstance())       return false;
#ifdef CHE_DEBUG
		CreateDebugMessenger();
#endif
		if (!CreateSurface(window))  return false;
		if (!PickPhysicalDevice())   return false;
		if (!CreateLogicalDevice())  return false;

		// Загружаем функции dynamic rendering после создания устройства
		LoadDynamicRenderingFunctions();

		if (!m_vkCmdBeginRendering || !m_vkCmdEndRendering) {
			CHE_CORE_CRITICAL("VulkanContext: dynamic rendering functions not available");
			return false;
		}

		if (!CreateSwapchain(width, height)) return false;
		if (!CreateDepthResources()) return false;
		if (!CreateCommandPool())    return false;
		if (!CreateCommandBuffers()) return false;
		if (!CreateSyncObjects())    return false;

		CHE_CORE_INFO("Vulkan initialized successfully");
		return true;
	}

	void VulkanContext::Shutdown()
	{
		if (m_Device) vkDeviceWaitIdle(m_Device);

		CleanupSwapchain();

		for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			if (m_RenderFinishedSemaphores.size() > i)
				vkDestroySemaphore(m_Device, m_RenderFinishedSemaphores[i], nullptr);
			if (m_ImageAvailableSemaphores.size() > i)
				vkDestroySemaphore(m_Device, m_ImageAvailableSemaphores[i], nullptr);
			if (m_InFlightFences.size() > i)
				vkDestroyFence(m_Device, m_InFlightFences[i], nullptr);
		}

		if (m_CommandPool) vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
		if (m_Device)      vkDestroyDevice(m_Device, nullptr);

#ifdef CHE_DEBUG
		DestroyDebugMessenger();
#endif
		if (m_Surface)  vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
		if (m_Instance) vkDestroyInstance(m_Instance, nullptr);
	}

	// ─── Per-frame ────────────────────────────────────────────────────────────
	bool VulkanContext::BeginFrame()
	{
		vkWaitForFences(m_Device, 1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);

		VkResult result = vkAcquireNextImageKHR(m_Device, m_Swapchain, UINT64_MAX,
			m_ImageAvailableSemaphores[m_CurrentFrame], VK_NULL_HANDLE, &m_CurrentImageIdx);

		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			RecreateSwapchain(m_SwapchainExtent.width, m_SwapchainExtent.height);
			return false;
		}

		vkResetFences(m_Device, 1, &m_InFlightFences[m_CurrentFrame]);

		VkCommandBuffer cmd = m_CommandBuffers[m_CurrentFrame];
		vkResetCommandBuffer(cmd, 0);

		VkCommandBufferBeginInfo beginInfo = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		};
		vkBeginCommandBuffer(cmd, &beginInfo);

		// Transition swapchain image to color attachment
		VkImageMemoryBarrier imageBarrier = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = 0,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.image = m_SwapchainImages[m_CurrentImageIdx],
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		};

		vkCmdPipelineBarrier(cmd,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

		// Transition depth image
		VkImageMemoryBarrier depthBarrier = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = 0,
			.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			.image = m_DepthImage,
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		};

		vkCmdPipelineBarrier(cmd,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
			0, 0, nullptr, 0, nullptr, 1, &depthBarrier);

		// Begin dynamic rendering
		VkRenderingAttachmentInfo colorAttachment = {
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = m_SwapchainImageViews[m_CurrentImageIdx],
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = {
				.color = {{m_ClearR, m_ClearG, m_ClearB, m_ClearA}},
			},
		};

		VkRenderingAttachmentInfo depthAttachment = {
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = m_DepthImageView,
			.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.clearValue = {
				.depthStencil = {1.0f, 0},
			},
		};

		VkRenderingInfo renderingInfo = {
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.renderArea = {
				.offset = {0, 0},
				.extent = m_SwapchainExtent,
			},
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &colorAttachment,
			.pDepthAttachment = &depthAttachment,
		};

		m_vkCmdBeginRendering(cmd, &renderingInfo);

		VkViewport viewport = {
			.x = 0.0f,
			.y = 0.0f,
			.width = static_cast<float>(m_SwapchainExtent.width),
			.height = static_cast<float>(m_SwapchainExtent.height),
			.minDepth = 0.0f,
			.maxDepth = 1.0f,
		};
		vkCmdSetViewport(cmd, 0, 1, &viewport);

		VkRect2D scissor = {
			.offset = {0, 0},
			.extent = m_SwapchainExtent,
		};
		vkCmdSetScissor(cmd, 0, 1, &scissor);

		return true;
	}

	void VulkanContext::EndFrame()
	{
		VkCommandBuffer cmd = m_CommandBuffers[m_CurrentFrame];

		m_vkCmdEndRendering(cmd);

		// Transition swapchain image to present
		VkImageMemoryBarrier presentBarrier = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = 0,
			.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			.image = m_SwapchainImages[m_CurrentImageIdx],
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		};

		vkCmdPipelineBarrier(cmd,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			0, 0, nullptr, 0, nullptr, 1, &presentBarrier);

		vkEndCommandBuffer(cmd);

		VkSubmitInfo submitInfo = {
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		};

		VkSemaphore waitSemaphores[] = { m_ImageAvailableSemaphores[m_CurrentFrame] };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmd;

		VkSemaphore signalSemaphores[] = { m_RenderFinishedSemaphores[m_CurrentFrame] };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, m_InFlightFences[m_CurrentFrame]);

		VkPresentInfoKHR presentInfo = {
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = signalSemaphores,
			.swapchainCount = 1,
			.pSwapchains = &m_Swapchain,
			.pImageIndices = &m_CurrentImageIdx,
		};

		VkResult result = vkQueuePresentKHR(m_PresentQueue, &presentInfo);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
			RecreateSwapchain(m_SwapchainExtent.width, m_SwapchainExtent.height);

		m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	void VulkanContext::SetViewport(uint32_t /*width*/, uint32_t /*height*/)
	{
		// Viewport устанавливается в BeginFrame через dynamic state
	}

	void VulkanContext::RecreateSwapchain(uint32_t width, uint32_t height)
	{
		vkDeviceWaitIdle(m_Device);
		CleanupSwapchain();

		if (!CreateSwapchain(width, height)) {
			CHE_CORE_ERROR("VulkanContext: Failed to recreate swapchain");
			return;
		}
		if (!CreateDepthResources()) {
			CHE_CORE_ERROR("VulkanContext: Failed to recreate depth resources");
			return;
		}
	}

	void VulkanContext::SetClearColor(float r, float g, float b, float a)
	{
		m_ClearR = r;
		m_ClearG = g;
		m_ClearB = b;
		m_ClearA = a;
	}

	// ─── Create functions ─────────────────────────────────────────────────────

	bool VulkanContext::CreateInstance()
	{
		GetInstance();

		VkApplicationInfo appInfo = {
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pApplicationName = "CHEngine",
			.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
			.pEngineName = "CHEngine",
			.engineVersion = VK_MAKE_VERSION(1, 0, 0),
			.apiVersion = VK_MAKE_API_VERSION(0, m_InstanceVersion.Major, m_InstanceVersion.Minor, 0),
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
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pApplicationInfo = &appInfo,
			.enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
			.ppEnabledExtensionNames = extensions.data(),
		};

#ifdef CHE_PLATFORM_APPLE
		createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

#ifdef CHE_DEBUG
		createInfo.enabledLayerCount = static_cast<uint32_t>(s_ValidationLayers.size());
		createInfo.ppEnabledLayerNames = s_ValidationLayers.data();
#else
		createInfo.enabledLayerCount = 0;
#endif
		{
			VkResult result = vkCreateInstance(&createInfo, nullptr, &m_Instance);
			VK_CHECK_RESULT(result, "Create instance");
		}
		return true;
	}

	bool VulkanContext::CreateSurface(GLFWwindow* window)
	{
		VkResult result = glfwCreateWindowSurface(m_Instance, window, nullptr, &m_Surface);
		VK_CHECK_RESULT(result, "Create surface");
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

			if (foundGraphics && foundPresent) {
				VkPhysicalDeviceProperties props;
				vkGetPhysicalDeviceProperties(device, &props);

				// Проверяем поддержку dynamic rendering
				const bool isVulkan13 = props.apiVersion >= VK_API_VERSION_1_3;

				if (!isVulkan13) {
					// Для Vulkan < 1.3 проверяем расширение
					uint32_t extCount;
					vkEnumerateDeviceExtensionProperties(device, nullptr, &extCount, nullptr);
					std::vector<VkExtensionProperties> availableExtensions(extCount);
					vkEnumerateDeviceExtensionProperties(device, nullptr, &extCount, availableExtensions.data());

					bool hasDynamicRendering = false;
					for (const auto& ext : availableExtensions) {
						if (std::strcmp(ext.extensionName, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME) == 0) {
							hasDynamicRendering = true;
							break;
						}
					}

					if (!hasDynamicRendering) {
						CHE_CORE_WARN("Vulkan GPU {} (API {}.{}.{}) doesn't support dynamic rendering extension, skipping",
							props.deviceName,
							VK_API_VERSION_MAJOR(props.apiVersion),
							VK_API_VERSION_MINOR(props.apiVersion),
							VK_API_VERSION_PATCH(props.apiVersion));
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
		}

		CHE_CORE_CRITICAL("VulkanContext: suitable GPU with dynamic rendering support not found");
		return false;
	}

	bool VulkanContext::CreateLogicalDevice()
	{
		std::set<uint32_t> uniqueQueueFamilies = { m_GraphicsQueueFamily, m_PresentQueueFamily };

		float queuePriority = 1.0f;
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		for (uint32_t queueFamily : uniqueQueueFamilies) {
			VkDeviceQueueCreateInfo queueCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.queueFamilyIndex = queueFamily,
				.queueCount = 1,
				.pQueuePriorities = &queuePriority,
			};
			queueCreateInfos.push_back(queueCreateInfo);
		}

		std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

		// Для Vulkan < 1.3 добавляем расширение dynamic rendering
		if (m_InstanceVersion.Major < 1 || (m_InstanceVersion.Major == 1 && m_InstanceVersion.Minor < 3)) {
			deviceExtensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
		}

#ifdef CHE_PLATFORM_APPLE
		deviceExtensions.push_back("VK_KHR_portability_subset");
#endif

		// Активируем dynamic rendering через pNext цепочку
		VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
			.dynamicRendering = VK_TRUE,
		};

		VkPhysicalDeviceFeatures2 deviceFeatures2 = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
			.pNext = &dynamicRenderingFeatures,
		};

		VkDeviceCreateInfo createInfo = {
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.pNext = &deviceFeatures2,
			.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
			.pQueueCreateInfos = queueCreateInfos.data(),
			.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
			.ppEnabledExtensionNames = deviceExtensions.data(),
		};

		{
			VkResult res = vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device);
			VK_CHECK_RESULT(res, "Create device");
		}

		vkGetDeviceQueue(m_Device, m_GraphicsQueueFamily, 0, &m_GraphicsQueue);
		vkGetDeviceQueue(m_Device, m_PresentQueueFamily, 0, &m_PresentQueue);
		return true;
	}

	bool VulkanContext::CreateSwapchain(uint32_t width, uint32_t height)
	{
		VkSurfaceCapabilitiesKHR capabilities;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevice, m_Surface, &capabilities);

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &formatCount, nullptr);
		std::vector<VkSurfaceFormatKHR> formats(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &formatCount, formats.data());

		m_SwapchainFormat = formats[0].format;
		VkColorSpaceKHR colorSpace = formats[0].colorSpace;
		for (const auto& fmt : formats) {
			if (fmt.format == VK_FORMAT_B8G8R8A8_SRGB && fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				m_SwapchainFormat = fmt.format;
				colorSpace = fmt.colorSpace;
				break;
			}
		}

		VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
			m_SwapchainExtent = capabilities.currentExtent;
		}
		else {
			m_SwapchainExtent.width = std::clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			m_SwapchainExtent.height = std::clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
		}

		uint32_t imageCount = capabilities.minImageCount + 1;
		if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
			imageCount = capabilities.maxImageCount;

		VkSwapchainCreateInfoKHR swapchainInfo = {
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.surface = m_Surface,
			.minImageCount = imageCount,
			.imageFormat = m_SwapchainFormat,
			.imageColorSpace = colorSpace,
			.imageExtent = m_SwapchainExtent,
			.imageArrayLayers = 1,
			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		};

		uint32_t queueFamilyIndices[] = { m_GraphicsQueueFamily, m_PresentQueueFamily };
		if (m_GraphicsQueueFamily != m_PresentQueueFamily) {
			swapchainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			swapchainInfo.queueFamilyIndexCount = 2;
			swapchainInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else {
			swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		}

		swapchainInfo.preTransform = capabilities.currentTransform;
		swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		swapchainInfo.presentMode = presentMode;
		swapchainInfo.clipped = VK_TRUE;

		{
			VkResult result = vkCreateSwapchainKHR(m_Device, &swapchainInfo, nullptr, &m_Swapchain);
			VK_CHECK_RESULT(result, "Create Swapchain");
		}
		vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, nullptr);
		m_SwapchainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, m_SwapchainImages.data());

		m_SwapchainImageViews.resize(imageCount);
		for (uint32_t i = 0; i < imageCount; i++) {
			VkImageViewCreateInfo viewInfo = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.image = m_SwapchainImages[i],
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = m_SwapchainFormat,
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1,
				},
			};
			VkResult result = vkCreateImageView(m_Device, &viewInfo, nullptr, &m_SwapchainImageViews[i]);
			VK_CHECK_RESULT(result, "Create image view");
		}

		return true;
	}

	bool VulkanContext::CreateDepthResources()
	{
		VkFormat depthFormat = FindDepthFormat(m_PhysicalDevice);

		VkImageCreateInfo imageInfo = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.imageType = VK_IMAGE_TYPE_2D,
			.extent = {
				.width = m_SwapchainExtent.width,
				.height = m_SwapchainExtent.height,
				.depth = 1,
			},
			.mipLevels = 1,
			.arrayLayers = 1,
			.format = depthFormat,
			.tiling = VK_IMAGE_TILING_OPTIMAL,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			.samples = VK_SAMPLE_COUNT_1_BIT,
		};
		{
			VkResult result = vkCreateImage(m_Device, &imageInfo, nullptr, &m_DepthImage);
			VK_CHECK_RESULT(result, "Create depth image");
		}
		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements(m_Device, m_DepthImage, &memReqs);

		VkMemoryAllocateInfo allocInfo = {
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.allocationSize = memReqs.size,
			.memoryTypeIndex = FindMemoryType(m_PhysicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
		};
		{
			VkResult result = vkAllocateMemory(m_Device, &allocInfo, nullptr, &m_DepthMemory);
			VK_CHECK_RESULT(result, "Allocate depth memory");
		}
		vkBindImageMemory(m_Device, m_DepthImage, m_DepthMemory, 0);

		VkImageViewCreateInfo viewInfo = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = m_DepthImage,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = depthFormat,
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		};

		{
			VkResult result = vkCreateImageView(m_Device, &viewInfo, nullptr, &m_DepthImageView);
			VK_CHECK_RESULT(result, "Create depth image view");
		}
		return true;
	}

	void VulkanContext::GetInstance()
	{
		uint32_t instance_version;

		VkResult res = vkEnumerateInstanceVersion(&instance_version);
		VK_CHECK_RESULT(res, "Version instance");

		m_InstanceVersion = {
			.Major = VK_API_VERSION_MAJOR(instance_version),
			.Minor = VK_API_VERSION_MINOR(instance_version),
			.Patch = VK_API_VERSION_PATCH(instance_version),
		};

		CHE_CORE_INFO("Vulkan Instance Version: {}.{}.{}",
			m_InstanceVersion.Major, m_InstanceVersion.Minor, m_InstanceVersion.Patch);
	}

	bool VulkanContext::CreateCommandPool()
	{
		VkCommandPoolCreateInfo poolInfo = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = m_GraphicsQueueFamily,
		};
		{
			VkResult result = vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_CommandPool);
			VK_CHECK_RESULT(result, "Create command pool");
		}
		return true;
	}

	bool VulkanContext::CreateCommandBuffers()
	{
		m_CommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

		VkCommandBufferAllocateInfo allocInfo = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = m_CommandPool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = MAX_FRAMES_IN_FLIGHT,
		};
		{
			VkResult result = vkAllocateCommandBuffers(m_Device, &allocInfo, m_CommandBuffers.data());
			VK_CHECK_RESULT(result, "Allocate command buffers");
		}
		return true;
	}

	bool VulkanContext::CreateSyncObjects()
	{
		m_ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		m_RenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		m_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

		VkSemaphoreCreateInfo semInfo = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		};
		VkFenceCreateInfo fenceInfo = {
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT,
		};
		for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			{
				VkResult res = vkCreateSemaphore(m_Device, &semInfo, nullptr, &m_ImageAvailableSemaphores[i]);
				VK_CHECK_RESULT(res, "Create available semaphore");
			}
			{
				VkResult res = vkCreateSemaphore(m_Device, &semInfo, nullptr, &m_RenderFinishedSemaphores[i]);
				VK_CHECK_RESULT(res, "Create finished semaphore");
			}
			{
				VkResult res = vkCreateFence(m_Device, &fenceInfo, nullptr, &m_InFlightFences[i]);
				VK_CHECK_RESULT(res, "Create fence");
			}
		}
		return true;
	}

	void VulkanContext::CleanupSwapchain()
	{
		if (m_DepthImageView) { vkDestroyImageView(m_Device, m_DepthImageView, nullptr); m_DepthImageView = VK_NULL_HANDLE; }
		if (m_DepthImage) { vkDestroyImage(m_Device, m_DepthImage, nullptr);     m_DepthImage = VK_NULL_HANDLE; }
		if (m_DepthMemory) { vkFreeMemory(m_Device, m_DepthMemory, nullptr);      m_DepthMemory = VK_NULL_HANDLE; }

		for (auto iv : m_SwapchainImageViews)
			vkDestroyImageView(m_Device, iv, nullptr);
		m_SwapchainImageViews.clear();

		if (m_Swapchain) {
			vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
			m_Swapchain = VK_NULL_HANDLE;
		}
	}

}
