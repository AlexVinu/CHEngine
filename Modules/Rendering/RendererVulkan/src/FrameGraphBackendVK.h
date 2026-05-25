#pragma once

#include "Render/IFrameGraphBackend.h"
#include "Render/Graph/PassDesc.h"
#include "Render/Handles.h"

#include <vulkan/vulkan.h>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace CHModules {

    struct RenderFactoryVK;
    class  VulkanContext;

    /// Vulkan implementation of IFrameGraphBackend.
    /// Uses dynamic rendering (no VkRenderPass). Per-frame descriptor pools are
    /// owned by VulkanContext and reset at BeginFrame.
    ///
    /// OnGraphCompiled(): pre-bakes image barriers per pass and warms up pipelines.
    /// Execute(): records all passes into the current frame's command buffer.
    class FrameGraphBackendVK final : public CHEngine::IFrameGraphBackend
    {
    public:
        FrameGraphBackendVK(RenderFactoryVK& factory, VulkanContext& ctx);
        ~FrameGraphBackendVK() override = default;

        void OnGraphCompiled(const Vector<CHEngine::PassDesc>& passes) override;
        void Execute(const Vector<CHEngine::PassDesc>& passes) override;

    private:
        // ─── Pre-baked per-pass data (from OnGraphCompiled) ───────────────────
        struct PassBarriers {
            // Barriers to issue before vkCmdBeginRendering for this pass.
            std::vector<VkImageMemoryBarrier> pre;
            // Barrier src/dst stages for the pre-barrier batch.
            VkPipelineStageFlags preSrc = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            VkPipelineStageFlags preDst = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                          VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT    |
                                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        };
        std::vector<PassBarriers> m_PrebakedBarriers;

        // Tracks the layout + VkImage of each texture handle (by index) across the compiled graph.
        // Storing VkImage here lets us bake finalization barriers at the end of OnGraphCompiled.
        struct LayoutEntry {
            VkImageLayout      layout = VK_IMAGE_LAYOUT_UNDEFINED;
            VkImage            image  = VK_NULL_HANDLE;
            VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        };
        std::unordered_map<uint32_t, LayoutEntry> m_CompiledLayouts;

        // Post-execute barriers: transition color render targets from COLOR_ATTACHMENT_OPTIMAL
        // to SHADER_READ_ONLY_OPTIMAL so ImGui can sample them after Execute().
        std::vector<VkImageMemoryBarrier> m_FinalizeBarriers;

        // ─── Execute helpers ──────────────────────────────────────────────────
        void ExecutePass(VkCommandBuffer cmd, const CHEngine::PassDesc& pass,
                         const PassBarriers* prebaked);

        void IssueBarrier(VkCommandBuffer cmd,
                          VkImage image,
                          VkImageLayout from, VkImageLayout to,
                          VkImageAspectFlags aspect);

        VkDescriptorSet AllocateAndWriteSet(const CHEngine::PassDesc& pass,
                                            const CHEngine::DrawDesc* draw);

        static VkAttachmentLoadOp  ToVkLoadOp(CHEngine::ELoadOp op);
        static VkAttachmentStoreOp ToVkStoreOp(CHEngine::EStoreOp op);
        static VkImageAspectFlags  GuessAspect(VkFormat fmt);

        RenderFactoryVK& m_Factory;
        VulkanContext&   m_Ctx;

        // Layout tracking during Execute (per-frame runtime state).
        // Mirrors LayoutEntry but built at runtime (fallback path).
        struct RuntimeEntry {
            VkImageLayout      layout = VK_IMAGE_LAYOUT_UNDEFINED;
            VkImage            image  = VK_NULL_HANDLE;
            VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        };
        std::unordered_map<uint32_t, RuntimeEntry> m_RuntimeLayouts;
    };

} // namespace CHModules
