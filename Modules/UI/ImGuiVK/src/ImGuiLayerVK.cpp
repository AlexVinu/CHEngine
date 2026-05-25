#include "chepch.h"
#include "ImGuiLayerVK.h"

#include <Log/Log.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace CHModules {

    ImGuiLayerVK::ImGuiLayerVK(CHEngine::IWindow* window, CHEngine::IRenderFactory* renderFactory)
    {
        if (!window) {
            CHE_CORE_ERROR("ImGuiLayerVK: window is null");
            return;
        }

        GLFWwindow* glfwWin = static_cast<GLFWwindow*>(window->GetNativeWindow());
        if (!glfwWin) {
            CHE_CORE_ERROR("ImGuiLayerVK: GetNativeWindow() returned null");
            return;
        }

        if (!renderFactory) {
            CHE_CORE_ERROR("ImGuiLayerVK: renderFactory is null");
            return;
        }

        m_VkCtx = static_cast<VulkanSharedContext*>(renderFactory->GetNativeSharedContext());
        if (!m_VkCtx || !m_VkCtx->Device) {
            CHE_CORE_ERROR("ImGuiLayerVK: VulkanSharedContext not populated — RendererVK must be initialised first");
            m_VkCtx = nullptr;
            return;
        }

        VkDevice device = static_cast<VkDevice>(m_VkCtx->Device);

        // Load dynamic rendering function pointers.
        m_BeginRendering = reinterpret_cast<PFN_vkCmdBeginRendering>(
            vkGetDeviceProcAddr(device, "vkCmdBeginRendering"));
        if (!m_BeginRendering)
            m_BeginRendering = reinterpret_cast<PFN_vkCmdBeginRendering>(
                vkGetDeviceProcAddr(device, "vkCmdBeginRenderingKHR"));

        m_EndRendering = reinterpret_cast<PFN_vkCmdEndRendering>(
            vkGetDeviceProcAddr(device, "vkCmdEndRendering"));
        if (!m_EndRendering)
            m_EndRendering = reinterpret_cast<PFN_vkCmdEndRendering>(
                vkGetDeviceProcAddr(device, "vkCmdEndRenderingKHR"));

        if (!m_BeginRendering || !m_EndRendering) {
            CHE_CORE_ERROR("ImGuiLayerVK: vkCmdBeginRendering/vkCmdEndRendering not available — "
                           "VK_KHR_dynamic_rendering or Vulkan 1.3 required");
            return;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForVulkan(glfwWin, true);

        VkFormat colorFmt = static_cast<VkFormat>(m_VkCtx->SwapchainFormat);

        VkPipelineRenderingCreateInfoKHR renderingCI{};
        renderingCI.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        renderingCI.colorAttachmentCount    = 1;
        renderingCI.pColorAttachmentFormats = &colorFmt;

        ImGui_ImplVulkan_InitInfo initInfo{};
        initInfo.ApiVersion      = VK_API_VERSION_1_2;
        initInfo.Instance        = static_cast<VkInstance>(m_VkCtx->Instance);
        initInfo.PhysicalDevice  = static_cast<VkPhysicalDevice>(m_VkCtx->PhysicalDevice);
        initInfo.Device          = device;
        initInfo.QueueFamily     = m_VkCtx->QueueFamily;
        initInfo.Queue           = static_cast<VkQueue>(m_VkCtx->Queue);
        initInfo.DescriptorPoolSize = 64;
        initInfo.MinImageCount   = m_VkCtx->MinImageCount;
        initInfo.ImageCount      = m_VkCtx->ImageCount;
        initInfo.UseDynamicRendering = true;
        initInfo.PipelineInfoMain.PipelineRenderingCreateInfo = renderingCI;

        if (!ImGui_ImplVulkan_Init(&initInfo)) {
            CHE_CORE_ERROR("ImGuiLayerVK: ImGui_ImplVulkan_Init failed");
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
            return;
        }

        // Publish texture-registration helpers so RendererVK can wrap VkImageView as ImTextureID.
        m_VkCtx->RegisterImGuiTexture =
            [](void* sampler, void* imageView, uint32_t imageLayout) -> uint64_t {
                VkDescriptorSet set = ImGui_ImplVulkan_AddTexture(
                    static_cast<VkSampler>(sampler),
                    static_cast<VkImageView>(imageView),
                    static_cast<VkImageLayout>(imageLayout));
                return reinterpret_cast<uint64_t>(set);
            };
        m_VkCtx->UnregisterImGuiTexture =
            [](uint64_t descriptorSet) {
                if (descriptorSet)
                    ImGui_ImplVulkan_RemoveTexture(
                        reinterpret_cast<VkDescriptorSet>(descriptorSet));
            };

        m_Initialized = true;
        CHE_CORE_INFO("ImGuiLayerVK initialized");
    }

    ImGuiLayerVK::~ImGuiLayerVK()
    {
        if (m_Initialized) {
            if (m_VkCtx) {
                m_VkCtx->RegisterImGuiTexture   = nullptr;
                m_VkCtx->UnregisterImGuiTexture = nullptr;
            }
            ImGui_ImplVulkan_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
        }
    }

    void ImGuiLayerVK::Begin()
    {
        if (!m_Initialized) return;
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiLayerVK::End()
    {
        if (!m_Initialized || !m_VkCtx) return;
        ImGui::Render();

        VkCommandBuffer cmd  = static_cast<VkCommandBuffer>(m_VkCtx->CurrentCmdBuffer);
        VkImageView     view = static_cast<VkImageView>(m_VkCtx->CurrentSwapchainView);
        if (!cmd || !view) return;

        VkRenderingAttachmentInfoKHR colorAtt{};
        colorAtt.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
        colorAtt.imageView   = view;
        colorAtt.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAtt.loadOp      = VK_ATTACHMENT_LOAD_OP_LOAD;   // preserve 3D scene
        colorAtt.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;

        VkRenderingInfoKHR renderingInfo{};
        renderingInfo.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
        renderingInfo.renderArea.offset    = { 0, 0 };
        renderingInfo.renderArea.extent    = { m_VkCtx->SwapchainWidth, m_VkCtx->SwapchainHeight };
        renderingInfo.layerCount           = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments    = &colorAtt;

        m_BeginRendering(cmd, &renderingInfo);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
        m_EndRendering(cmd);
    }

} // namespace CHModules
