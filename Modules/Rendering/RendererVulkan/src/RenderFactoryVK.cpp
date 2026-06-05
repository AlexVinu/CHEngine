#include "RenderFactoryVK.h"
#include "FrameGraphBackendVK.h"
#include "VulkanGlobals.h"

#include <Log/Log.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace CHModules {

    // ─── Lifecycle ────────────────────────────────────────────────────────────

    void RenderFactoryVK::Init(const CHEngine::RendererInitInfo& init)
    {
        auto* window = static_cast<GLFWwindow*>(init.Vulkan.WindowHandle);
        if (!window) {
            CHE_CORE_CRITICAL("RenderFactoryVK::Init — WindowHandle is NULL");
            return;
        }

        int w = 0, h = 0;
        glfwGetFramebufferSize(window, &w, &h);

        if (!m_Context.Init(window, static_cast<uint32_t>(w), static_cast<uint32_t>(h))) {
            CHE_CORE_CRITICAL("RenderFactoryVK::Init — VulkanContext init failed");
            return;
        }

        VKGlobals::g_ContextPtr = &m_Context;

        // Publish Vulkan handles for ImGuiVK (and other modules that need them).
        {
            auto& vk            = VKGlobals::g_SharedContext;
            vk.Instance         = m_Context.GetInstance();
            vk.PhysicalDevice   = m_Context.GetPhysicalDevice();
            vk.Device           = m_Context.GetDevice();
            vk.Queue            = m_Context.GetGraphicsQueue();
            vk.QueueFamily      = m_Context.GetGraphicsQueueFamily();
            vk.ImageCount       = m_Context.GetSwapchainImageCount();
            vk.MinImageCount    = 2u;
            vk.SwapchainFormat  = static_cast<uint32_t>(m_Context.GetSwapchainFormat());
            VkExtent2D ext      = m_Context.GetSwapchainExtent();
            vk.SwapchainWidth   = ext.width;
            vk.SwapchainHeight  = ext.height;
        }

        // Query min UBO offset alignment.
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(m_Context.GetPhysicalDevice(), &props);
        m_UboAlignment = static_cast<uint32_t>(props.limits.minUniformBufferOffsetAlignment);

        Slang.Init(CHEngine::ERenderAPI::VULKAN);

        m_Initialized = true;
        CHE_CORE_INFO("RenderFactoryVK initialized");
    }

    void RenderFactoryVK::Shutdown()
    {
        if (!m_Initialized) return;

        // GPU idle before destroying any resources.
        vkDeviceWaitIdle(m_Context.GetDevice());

        Pipelines.Clear();
        Buffers.Clear();
        Textures.Clear();
        Shaders.Clear();

        VKGlobals::g_SharedContext = {};
        m_Context.Shutdown();  // flushes deletion queues — g_ContextPtr must still be valid
        VKGlobals::g_ContextPtr = nullptr;
        m_Initialized = false;
    }

    bool RenderFactoryVK::BeginFrame()
    {
        // Frame skipped (window minimized / 0×0 / swapchain out-of-date): do not
        // touch shared state or let the caller record/submit anything.
        if (!m_Context.BeginFrame())
            return false;

        // Keep per-frame shared state up to date for ImGuiVK.
        auto& vk                 = VKGlobals::g_SharedContext;
        vk.CurrentCmdBuffer      = m_Context.GetCurrentCommandBuffer();
        vk.CurrentSwapchainView  = m_Context.GetCurrentSwapchainImageView();
        VkExtent2D ext           = m_Context.GetSwapchainExtent();
        vk.SwapchainWidth        = ext.width;
        vk.SwapchainHeight       = ext.height;
        return true;
    }

    void RenderFactoryVK::EndFrame()
    {
        m_Context.EndFrame();
    }

    // ─── Buffer ───────────────────────────────────────────────────────────────

    CHEngine::BufferHandle RenderFactoryVK::CreateBuffer(
        uint64_t size,
        CHEngine::BufferUsage usage,
        CHEngine::MemoryType memory,
        std::span<const std::byte> initialData,
        const String&)
    {
        auto* buf = new BufferVK(size, usage, memory, initialData);
        return Buffers.Add(buf);
    }

    void RenderFactoryVK::Delete(CHEngine::BufferHandle h)
    {
        if (auto buf = Buffers.Take(h))
            m_Context.EnqueueDeletion(std::shared_ptr<void>(std::move(buf)));
    }

    void RenderFactoryVK::UpdateBuffer(CHEngine::BufferHandle h,
                                       std::span<const std::byte> data,
                                       uint64_t offset)
    {
        BufferVK* buf = Buffers.Get(h);
        if (buf) buf->UpdateData(data, offset);
    }

    // ─── Shader ───────────────────────────────────────────────────────────────

    CHEngine::ShaderHandle RenderFactoryVK::CreateShader(
        const String& slangSource,
        const String& vertEntry,
        const String& fragEntry,
        const String& sourcePath)
    {
        auto* sh = new ShaderVK(slangSource, vertEntry, fragEntry, sourcePath, Slang);
        return Shaders.Add(sh);
    }

    void RenderFactoryVK::Delete(CHEngine::ShaderHandle h)
    {
        if (auto sh = Shaders.Take(h))
            m_Context.EnqueueDeletion(std::shared_ptr<void>(std::move(sh)));
    }

    bool RenderFactoryVK::ReloadShader(CHEngine::ShaderHandle h,
                                       const String& slangSource,
                                       const String& vertEntry,
                                       const String& fragEntry,
                                       const String& sourcePath)
    {
        ShaderVK* sh = Shaders.Get(h);
        return sh && sh->Reload(slangSource, vertEntry, fragEntry, sourcePath);
    }

    // ─── Texture ──────────────────────────────────────────────────────────────

    CHEngine::TextureHandle RenderFactoryVK::CreateTexture(
        const uint8_t* data,
        uint32_t width,
        uint32_t height,
        uint32_t channels,
        uint32_t mipLevels,
        uint32_t arrayLayers,
        CHEngine::TextureFormat format,
        CHEngine::TextureType type,
        CHEngine::TextureUsage usage,
        CHEngine::MemoryType memory,
        const String& debugName)
    {
        auto* tex = new TextureVK(data, width, height, channels,
                                   mipLevels, arrayLayers,
                                   format, type, usage, memory,
                                   debugName.c_str());
        return Textures.Add(tex);
    }

    void RenderFactoryVK::Delete(CHEngine::TextureHandle h)
    {
        if (auto tex = Textures.Take(h))
            m_Context.EnqueueDeletion(std::shared_ptr<void>(std::move(tex)));
    }

    uint64_t RenderFactoryVK::GetTextureNativeID(CHEngine::TextureHandle h)
    {
        TextureVK* tex = Textures.Get(h);
        if (!tex) return 0u;

        // Vulkan ImGui needs a VkDescriptorSet (from ImGui_ImplVulkan_AddTexture),
        // not a raw VkImage. Lazily create and cache it on first call.
        if (tex->ImGuiDescSet() == VK_NULL_HANDLE) {
            auto& vk = VKGlobals::g_SharedContext;
            if (vk.RegisterImGuiTexture) {
                tex->ImGuiDescSet() = reinterpret_cast<VkDescriptorSet>(
                    vk.RegisterImGuiTexture(tex->GetSampler(),
                                            tex->GetView(),
                                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
            }
        }
        return reinterpret_cast<uint64_t>(tex->ImGuiDescSet());
    }

    // ─── Pipeline ─────────────────────────────────────────────────────────────

    CHEngine::PipelineHandle RenderFactoryVK::CreatePipeline(CHEngine::PipelineDesc desc)
    {
        auto* pip = new PipelineVK(desc, *this);
        return Pipelines.Add(pip);
    }

    void RenderFactoryVK::Delete(CHEngine::PipelineHandle h)
    {
        if (auto pip = Pipelines.Take(h))
            m_Context.EnqueueDeletion(std::shared_ptr<void>(std::move(pip)));
    }

    // ─── Frame-graph backend ──────────────────────────────────────────────────

    std::unique_ptr<CHEngine::IFrameGraphBackend> RenderFactoryVK::CreateFrameGraphBackend()
    {
        return std::make_unique<FrameGraphBackendVK>(*this, m_Context);
    }

    // ─── Misc ─────────────────────────────────────────────────────────────────

    bool RenderFactoryVK::CheckIsWorking()
    {
        // If already initialized, check the live device.
        if (m_Initialized)
            return m_Context.GetDevice() != VK_NULL_HANDLE;

        // Pre-init probe: create a minimal VkInstance, enumerate GPUs, destroy it.
        // Called by the engine before Init() to verify Vulkan is available at all.
        VkApplicationInfo appInfo = {};
        appInfo.sType      = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.apiVersion = VK_API_VERSION_1_2;

        VkInstanceCreateInfo info = {};
        info.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        info.pApplicationInfo = &appInfo;

        VkInstance probe = VK_NULL_HANDLE;
        if (vkCreateInstance(&info, nullptr, &probe) != VK_SUCCESS)
            return false;

        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(probe, &deviceCount, nullptr);
        vkDestroyInstance(probe, nullptr);

        return deviceCount > 0;
    }

    VkDevice RenderFactoryVK::GetDevice() const
    {
        return m_Context.GetDevice();
    }

    // ─── API-agnostic backend traits ──────────────────────────────────────────

    void RenderFactoryVK::WaitIdle()
    {
        if (m_Initialized && m_Context.GetDevice() != VK_NULL_HANDLE)
            vkDeviceWaitIdle(m_Context.GetDevice());
    }

    glm::mat4 RenderFactoryVK::GetClipSpaceCorrection() const
    {
        // Y-coord обрабатывается через negative viewport height в
        // FrameGraphBackendVK (см. там) — здесь только Z-remap [-1,1] → [0,1].
        // Иначе клиентские пайплайны с CullMode::Back увидели бы перевёрнутый
        // winding (передние грани становились бы задними и культились).
        glm::mat4 m(1.0f);
        m[2][2] = 0.5f;
        m[3][2] = 0.5f;
        return m;
    }

    bool RenderFactoryVK::SwapchainIsSRGB() const
    {
        if (!m_Initialized) return false;
        const VkFormat f = m_Context.GetSwapchainFormat();
        switch (f) {
            case VK_FORMAT_B8G8R8A8_SRGB:
            case VK_FORMAT_R8G8B8A8_SRGB:
            case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
                return true;
            default:
                return false;
        }
    }

    void* RenderFactoryVK::GetNativeSharedContext()
    {
        return &VKGlobals::g_SharedContext;
    }

} // namespace CHModules

IMPLEMENT_MODULE_FACTORY(CHModules::RenderFactoryVK)
