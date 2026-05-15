#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>

struct GLFWwindow;

namespace CHModules {

	// Централизованный Vulkan-контекст: instance, device, swapchain (dynamic rendering).
	// Создаётся один раз при инициализации RenderApiVK.
	class VulkanContext
	{
	public:
		bool Init(GLFWwindow* window, uint32_t width, uint32_t height);
		void Shutdown();

		// ─── Per-frame ──────────────────────────────────────────────────────
		bool BeginFrame();
		void EndFrame();

		// ─── Accessors ──────────────────────────────────────────────────────
		VkDevice         GetDevice()         const { return m_Device; }
		VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
		VkInstance       GetInstance()       const { return m_Instance; }
		VkCommandBuffer  GetCurrentCommandBuffer() const { return m_CommandBuffers[m_CurrentFrame]; }
		VkQueue          GetGraphicsQueue()  const { return m_GraphicsQueue; }
		uint32_t         GetGraphicsQueueFamily() const { return m_GraphicsQueueFamily; }
		VkExtent2D       GetSwapchainExtent() const { return m_SwapchainExtent; }
		VkFormat         GetSwapchainFormat() const { return m_SwapchainFormat; }

		void SetViewport(uint32_t width, uint32_t height);
		void RecreateSwapchain(uint32_t width, uint32_t height);

		void SetClearColor(float r, float g, float b, float a);

	private:
		bool CreateInstance();
		bool CreateSurface(GLFWwindow* window);
		bool PickPhysicalDevice();
		bool CreateLogicalDevice();
		bool CreateSwapchain(uint32_t width, uint32_t height);
		bool CreateCommandPool();
		bool CreateCommandBuffers();
		bool CreateSyncObjects();
		bool CreateDepthResources();

		void GetInstance();
		void LoadDynamicRenderingFunctions();
		void CleanupSwapchain();

		struct {
			int Major = 0;
			int Minor = 0;
			int Patch = 0;
		} m_InstanceVersion;

		VkPhysicalDeviceDynamicRenderingFeatures m_DynamicRenderingFeatures{};
		PFN_vkCmdBeginRendering m_vkCmdBeginRendering = nullptr;
		PFN_vkCmdEndRendering   m_vkCmdEndRendering = nullptr;

		VkInstance               m_Instance = VK_NULL_HANDLE;
		VkSurfaceKHR             m_Surface = VK_NULL_HANDLE;
		VkPhysicalDevice         m_PhysicalDevice = VK_NULL_HANDLE;
		VkDevice                 m_Device = VK_NULL_HANDLE;
		VkQueue                  m_GraphicsQueue = VK_NULL_HANDLE;
		VkQueue                  m_PresentQueue = VK_NULL_HANDLE;
		uint32_t                 m_GraphicsQueueFamily = 0;
		uint32_t                 m_PresentQueueFamily = 0;

		VkSwapchainKHR           m_Swapchain = VK_NULL_HANDLE;
		VkFormat                 m_SwapchainFormat = VK_FORMAT_B8G8R8A8_SRGB;
		VkExtent2D               m_SwapchainExtent = { 0, 0 };
		std::vector<VkImage>     m_SwapchainImages;
		std::vector<VkImageView> m_SwapchainImageViews;

		VkCommandPool            m_CommandPool = VK_NULL_HANDLE;

		// Depth buffer
		VkImage                  m_DepthImage = VK_NULL_HANDLE;
		VkDeviceMemory           m_DepthMemory = VK_NULL_HANDLE;
		VkImageView              m_DepthImageView = VK_NULL_HANDLE;

		static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
		std::vector<VkCommandBuffer> m_CommandBuffers;
		std::vector<VkSemaphore> m_ImageAvailableSemaphores;
		std::vector<VkSemaphore> m_RenderFinishedSemaphores;
		std::vector<VkFence>     m_InFlightFences;

		uint32_t m_CurrentFrame = 0;
		uint32_t m_CurrentImageIdx = 0;

#ifdef CHE_DEBUG
		VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
		bool CreateDebugMessenger();
		void DestroyDebugMessenger();
#endif

		float m_ClearR = 0.18f, m_ClearG = 0.18f, m_ClearB = 0.20f, m_ClearA = 1.0f;
	};

}
