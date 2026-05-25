#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <memory>
#include <vector>
#include <cstdint>

struct GLFWwindow;

namespace CHModules {

    // Centralized Vulkan context: instance, device, swapchain, command buffers, sync, VMA.
    // Owned by RenderFactoryVK. FrameGraphBackendVK uses it for per-frame rendering.
    class VulkanContext
    {
    public:
        bool Init(GLFWwindow* window, uint32_t width, uint32_t height);
        void Shutdown();

        // ─── Per-frame ─────────────────────────────────────────────────────────
        // BeginFrame: fence wait, acquire swapchain image, begin command buffer,
        //             issue initial image layout transitions (UNDEFINED → COLOR_ATTACHMENT).
        // Returns false if swapchain needs recreation (FG backend should skip Execute).
        bool BeginFrame();

        // EndFrame: transition swapchain to PRESENT_SRC, end + submit command buffer, present.
        void EndFrame();

        // ─── Single-time command buffer ────────────────────────────────────────
        VkCommandBuffer BeginSingleTimeCommands();
        void            EndSingleTimeCommands(VkCommandBuffer cmd);

        // ─── Descriptor helpers ────────────────────────────────────────────────
        // Resets the current frame's descriptor pool (call at start of BeginFrame).
        // Allocates a descriptor set from the current frame's pool.
        VkDescriptorSet AllocateDescriptorSet(VkDescriptorSetLayout layout);

        // ─── Accessors ─────────────────────────────────────────────────────────
        VkDevice         GetDevice()         const { return m_Device; }
        VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
        VkInstance       GetInstance()       const { return m_Instance; }
        VkCommandBuffer  GetCurrentCommandBuffer() const { return m_CommandBuffers[m_CurrentFrame]; }
        VkQueue          GetGraphicsQueue()  const { return m_GraphicsQueue; }
        uint32_t         GetGraphicsQueueFamily() const { return m_GraphicsQueueFamily; }
        VkExtent2D       GetSwapchainExtent() const { return m_SwapchainExtent; }
        VkFormat         GetSwapchainFormat() const { return m_SwapchainFormat; }
        VkFormat         GetDepthFormat()    const { return m_DepthFormat; }
        VmaAllocator     GetAllocator()      const { return m_Allocator; }
        uint32_t         GetSwapchainImageCount() const { return static_cast<uint32_t>(m_SwapchainImages.size()); }

        // Swapchain image for the current frame (for FG backend to use as color attachment).
        VkImage     GetCurrentSwapchainImage()     const { return m_SwapchainImages[m_CurrentImageIdx]; }
        VkImageView GetCurrentSwapchainImageView() const { return m_SwapchainImageViews[m_CurrentImageIdx]; }

        // Dynamic rendering function pointers (may be extension or Vulkan 1.3 core).
        void CmdBeginRendering(VkCommandBuffer cmd, const VkRenderingInfo* info) const {
            if (m_vkCmdBeginRendering) m_vkCmdBeginRendering(cmd, info);
        }
        void CmdEndRendering(VkCommandBuffer cmd) const {
            if (m_vkCmdEndRendering) m_vkCmdEndRendering(cmd);
        }

        void RecreateSwapchain(uint32_t width, uint32_t height);

        // ─── Deferred deletion queue ───────────────────────────────────────────
        // Enqueue a GPU resource for deferred destruction.  The shared_ptr keeps the
        // object alive until the frame slot's fence has signalled (BeginFrame flush),
        // at which point the GPU has finished reading it and it can be safely destroyed.
        void EnqueueDeletion(std::shared_ptr<void> resource);

        static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
        static constexpr uint32_t MAX_DESCRIPTOR_SETS_PER_FRAME = 512;
        static constexpr uint32_t MAX_DESCRIPTORS_PER_TYPE = 1024;

    private:
        bool CreateInstance();
        bool CreateSurface(GLFWwindow* window);
        bool PickPhysicalDevice();
        bool CreateLogicalDevice();
        bool CreateAllocator();
        bool CreateSwapchain(uint32_t width, uint32_t height);
        bool CreateCommandPool();
        bool CreateCommandBuffers();
        bool CreateSyncObjects();
        bool CreateRenderFinishedSemaphores();
        void DestroyRenderFinishedSemaphores();
        bool CreateDepthResources();
        bool CreateDescriptorPools();

        void GetInstanceVersion();
        void LoadDynamicRenderingFunctions();
        void CleanupSwapchain();
        void DestroyDescriptorPools();

        struct {
            int Major = 0;
            int Minor = 0;
            int Patch = 0;
        } m_InstanceVersion;

        PFN_vkCmdBeginRendering m_vkCmdBeginRendering = nullptr;
        PFN_vkCmdEndRendering   m_vkCmdEndRendering   = nullptr;

        GLFWwindow*              m_Window         = nullptr;

        VkInstance               m_Instance       = VK_NULL_HANDLE;
        VkSurfaceKHR             m_Surface        = VK_NULL_HANDLE;
        VkPhysicalDevice         m_PhysicalDevice = VK_NULL_HANDLE;
        VkDevice                 m_Device         = VK_NULL_HANDLE;
        VkQueue                  m_GraphicsQueue  = VK_NULL_HANDLE;
        VkQueue                  m_PresentQueue   = VK_NULL_HANDLE;
        uint32_t                 m_GraphicsQueueFamily = 0;
        uint32_t                 m_PresentQueueFamily  = 0;

        VmaAllocator             m_Allocator = VK_NULL_HANDLE;

        VkSwapchainKHR           m_Swapchain       = VK_NULL_HANDLE;
        VkFormat                 m_SwapchainFormat = VK_FORMAT_B8G8R8A8_UNORM;
        VkExtent2D               m_SwapchainExtent = { 0, 0 };
        std::vector<VkImage>     m_SwapchainImages;
        std::vector<VkImageView> m_SwapchainImageViews;

        VkCommandPool            m_CommandPool = VK_NULL_HANDLE;

        // Depth buffer (swapchain-sized, managed by VMA).
        VkImage      m_DepthImage     = VK_NULL_HANDLE;
        VmaAllocation m_DepthAlloc    = VK_NULL_HANDLE;
        VkImageView  m_DepthImageView = VK_NULL_HANDLE;
        VkFormat     m_DepthFormat    = VK_FORMAT_D24_UNORM_S8_UINT;

        std::vector<VkCommandBuffer> m_CommandBuffers;
        std::vector<VkSemaphore> m_ImageAvailableSemaphores;
        std::vector<VkSemaphore> m_RenderFinishedSemaphores;
        std::vector<VkFence>     m_InFlightFences;

        // Per-frame descriptor pools, reset at start of each frame.
        std::vector<VkDescriptorPool> m_DescriptorPools;

        // Per-frame deletion queues: flushed when the matching fence signals (BeginFrame).
        std::vector<std::shared_ptr<void>> m_DeletionQueues[MAX_FRAMES_IN_FLIGHT];

        uint32_t m_CurrentFrame    = 0;
        uint32_t m_CurrentImageIdx = 0;

#ifdef CHE_DEBUG
        VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
        bool CreateDebugMessenger();
        void DestroyDebugMessenger();
#endif
    };

} // namespace CHModules
