#include "FrameGraphBackendVK.h"
#include "RenderFactoryVK.h"
#include "PipelineVK.h"
#include "BufferVK.h"
#include "TextureVK.h"
#include "VulkanContext.h"
#include "VKCore.h"

#include <Log/Log.h>
#include <algorithm>

namespace CHModules {

    FrameGraphBackendVK::FrameGraphBackendVK(RenderFactoryVK& factory, VulkanContext& ctx)
        : m_Factory(factory), m_Ctx(ctx)
    {
    }

    // ─── Helpers ─────────────────────────────────────────────────────────────

    VkAttachmentLoadOp FrameGraphBackendVK::ToVkLoadOp(CHEngine::ELoadOp op)
    {
        switch (op) {
        case CHEngine::ELoadOp::Load:    return VK_ATTACHMENT_LOAD_OP_LOAD;
        case CHEngine::ELoadOp::Clear:   return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case CHEngine::ELoadOp::DontCare: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        }
        return VK_ATTACHMENT_LOAD_OP_CLEAR;
    }

    VkAttachmentStoreOp FrameGraphBackendVK::ToVkStoreOp(CHEngine::EStoreOp op)
    {
        switch (op) {
        case CHEngine::EStoreOp::Store:   return VK_ATTACHMENT_STORE_OP_STORE;
        case CHEngine::EStoreOp::DontCare: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        }
        return VK_ATTACHMENT_STORE_OP_STORE;
    }

    VkImageAspectFlags FrameGraphBackendVK::GuessAspect(VkFormat fmt)
    {
        if (fmt == VK_FORMAT_D32_SFLOAT || fmt == VK_FORMAT_D16_UNORM)
            return VK_IMAGE_ASPECT_DEPTH_BIT;
        if (fmt == VK_FORMAT_D32_SFLOAT_S8_UINT || fmt == VK_FORMAT_D24_UNORM_S8_UINT)
            return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }

    void FrameGraphBackendVK::IssueBarrier(VkCommandBuffer cmd,
                                           VkImage image,
                                           VkImageLayout from, VkImageLayout to,
                                           VkImageAspectFlags aspect)
    {
        VkImageMemoryBarrier barrier = {
            .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask       = VK_ACCESS_MEMORY_WRITE_BIT,
            .dstAccessMask       = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
            .oldLayout           = from,
            .newLayout           = to,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image               = image,
            .subresourceRange    = { aspect, 0, 1, 0, 1 },
        };
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    // ─── OnGraphCompiled ──────────────────────────────────────────────────────
    // Pre-bake image layout transitions for all passes and warm up pipelines.

    void FrameGraphBackendVK::OnGraphCompiled(const Vector<CHEngine::PassDesc>& passes)
    {
        m_PrebakedBarriers.clear();
        m_CompiledLayouts.clear();

        // Warm up all pipelines so PSO compilation doesn't spike on the first frame.
        for (const auto& pass : passes) {
            if (pass.Pipeline.IsValid())
                m_Factory.Pipelines.Get(pass.Pipeline); // just touch to trigger lazy init if any
        }

        for (const auto& pass : passes) {
            PassBarriers pb;

            // Helper: emit a layout-transition barrier if needed; always updates m_CompiledLayouts.
            auto addBarrier = [&](CHEngine::TextureHandle th, VkImageLayout to) {
                if (!th.IsValid()) return;
                TextureVK* tex = m_Factory.Textures.Get(th);
                if (!tex) return;

                VkFormat fmt = tex->GetVkFormat();
                VkImageAspectFlags aspect = GuessAspect(fmt);
                VkImage image = tex->GetImage();

                VkImageLayout from = VK_IMAGE_LAYOUT_UNDEFINED;
                auto it = m_CompiledLayouts.find(th.index);
                if (it != m_CompiledLayouts.end()) from = it->second.layout;

                // Always update tracking with image+aspect so finalization can use them.
                m_CompiledLayouts[th.index] = { to, image, aspect };

                if (from == to) return; // already in the right layout — no barrier needed

                // srcAccess: what previous passes may have done to this image.
                VkAccessFlags srcAccess = 0;
                if (from != VK_IMAGE_LAYOUT_UNDEFINED) {
                    srcAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                              | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
                              | VK_ACCESS_SHADER_READ_BIT;
                }

                // dstAccess: what this pass will do.
                VkAccessFlags dstAccess = 0;
                switch (to) {
                case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                    dstAccess = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                              | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                    break;
                case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                    dstAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                              | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                    break;
                case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
                    dstAccess = VK_ACCESS_SHADER_READ_BIT;
                    break;
                default:
                    dstAccess = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
                    break;
                }

                VkImageMemoryBarrier b = {
                    .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    .srcAccessMask       = srcAccess,
                    .dstAccessMask       = dstAccess,
                    .oldLayout           = from,
                    .newLayout           = to,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .image               = image,
                    .subresourceRange    = { aspect, 0, 1, 0, 1 },
                };
                pb.pre.push_back(b);
            };

            // Helper to check if a handle belongs to the pass's attachment set.
            auto isAttachment = [&](CHEngine::TextureHandle th) -> bool {
                for (auto ca : pass.ColorAttachments)
                    if (ca.index == th.index) return true;
                if (pass.DepthAttachment.IsValid() && pass.DepthAttachment.index == th.index)
                    return true;
                return false;
            };

            // 1. Color attachments → COLOR_ATTACHMENT_OPTIMAL.
            for (CHEngine::TextureHandle th : pass.ColorAttachments)
                addBarrier(th, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

            // 2. Depth attachment → DEPTH_STENCIL_ATTACHMENT_OPTIMAL.
            if (pass.DepthAttachment.IsValid()) {
                TextureVK* dtex = m_Factory.Textures.Get(pass.DepthAttachment);
                if (dtex) {
                    const bool isDepth = (GuessAspect(dtex->GetVkFormat()) != VK_IMAGE_ASPECT_COLOR_BIT);
                    addBarrier(pass.DepthAttachment,
                        isDepth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                                : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
                }
            }

            // 3. Writes (topological sort hints) — catch write targets not listed as attachments.
            for (CHEngine::TextureHandle th : pass.Writes) {
                if (!th.IsValid()) continue;
                TextureVK* tex = m_Factory.Textures.Get(th);
                if (!tex) continue;
                VkFormat fmt = tex->GetVkFormat();
                VkImageLayout to = (GuessAspect(fmt) == VK_IMAGE_ASPECT_COLOR_BIT)
                    ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                    : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                addBarrier(th, to);
            }

            // 4. Reads → shader-read layout (skip textures that are also attachments this pass).
            for (CHEngine::TextureHandle th : pass.Reads) {
                if (!th.IsValid()) continue;
                if (isAttachment(th)) continue; // can't be read-only and attachment simultaneously

                TextureVK* tex = m_Factory.Textures.Get(th);
                if (!tex) continue;
                VkFormat fmt = tex->GetVkFormat();
                const bool isDepth = (GuessAspect(fmt) != VK_IMAGE_ASPECT_COLOR_BIT);
                addBarrier(th, isDepth
                    ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                    : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }

            m_PrebakedBarriers.push_back(std::move(pb));
        }

        // ── Finalization barriers ─────────────────────────────────────────────────
        // After all passes, color render targets are left in COLOR_ATTACHMENT_OPTIMAL.
        // ImGui samples these textures (via VkDescriptorSets registered by GetTextureNativeID).
        // We need to transition them to SHADER_READ_ONLY_OPTIMAL before ImGui's render pass.
        m_FinalizeBarriers.clear();
        for (auto& [idx, entry] : m_CompiledLayouts) {
            if (entry.layout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) continue;
            if (entry.image  == VK_NULL_HANDLE) continue;

            m_FinalizeBarriers.push_back({
                .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .srcAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT,
                .oldLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image               = entry.image,
                .subresourceRange    = { entry.aspect, 0, 1, 0, 1 },
            });
            // Update tracked layout — next OnGraphCompiled resets this anyway,
            // but keeps m_CompiledLayouts consistent if Execute runs without recompile.
            entry.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
    }

    // ─── AllocateAndWriteSet ─────────────────────────────────────────────────

    VkDescriptorSet FrameGraphBackendVK::AllocateAndWriteSet(
        const CHEngine::PassDesc& pass,
        const CHEngine::DrawDesc* draw)
    {
        PipelineVK* pipeline = m_Factory.Pipelines.Get(pass.Pipeline);
        if (!pipeline || pipeline->GetSetLayout() == VK_NULL_HANDLE)
            return VK_NULL_HANDLE;

        VkDescriptorSet set = m_Ctx.AllocateDescriptorSet(pipeline->GetSetLayout());
        if (set == VK_NULL_HANDLE) return VK_NULL_HANDLE;

        const DescriptorLayout& descLayout = pipeline->GetDescLayout();

        std::vector<VkWriteDescriptorSet> writes;
        std::vector<VkDescriptorBufferInfo> bufInfos; // stable storage
        std::vector<VkDescriptorImageInfo>  imgInfos;
        bufInfos.reserve(pass.Uniforms.size() + (draw ? draw->Uniforms.size() : 0));
        imgInfos.reserve(pass.Textures.size() + (draw ? draw->Textures.size() : 0));

        auto writeUbo = [&](const CHEngine::UniformBinding& ub) {
            if (ub.Slot >= descLayout.uboVkBindings.size()) return;
            BufferVK* buf = m_Factory.Buffers.Get(ub.Buffer);
            if (!buf) return;

            bufInfos.push_back({
                .buffer = buf->GetBuffer(),
                .offset = ub.Offset,
                .range  = (ub.Size > 0) ? ub.Size : VK_WHOLE_SIZE,
            });
            writes.push_back({
                .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet          = set,
                .dstBinding      = descLayout.uboVkBindings[ub.Slot],
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo     = &bufInfos.back(),
            });
        };

        auto writeTex = [&](const CHEngine::TextureBinding& tb) {
            if (tb.Slot >= descLayout.samplerVkBindings.size()) return;
            TextureVK* tex = m_Factory.Textures.Get(tb.Texture);
            if (!tex || tex->GetView() == VK_NULL_HANDLE) return;

            imgInfos.push_back({
                .sampler     = tex->GetSampler(),
                .imageView   = tex->GetView(),
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            });
            writes.push_back({
                .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet          = set,
                .dstBinding      = descLayout.samplerVkBindings[tb.Slot],
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo      = &imgInfos.back(),
            });
        };

        for (const auto& ub : pass.Uniforms) writeUbo(ub);
        for (const auto& tb : pass.Textures) writeTex(tb);
        if (draw) {
            for (const auto& ub : draw->Uniforms) writeUbo(ub);
            for (const auto& tb : draw->Textures) writeTex(tb);
        }

        if (!writes.empty())
            vkUpdateDescriptorSets(m_Ctx.GetDevice(),
                static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

        return set;
    }

    // ─── Execute ─────────────────────────────────────────────────────────────

    void FrameGraphBackendVK::Execute(const Vector<CHEngine::PassDesc>& passes)
    {
        // Reset per-frame layout tracking.
        m_RuntimeLayouts.clear();

        const bool hasPrebaked = (m_PrebakedBarriers.size() == passes.size());

        VkCommandBuffer cmd = m_Ctx.GetCurrentCommandBuffer();

        for (size_t i = 0; i < passes.size(); ++i) {
            const CHEngine::PassDesc* pass = &passes[i];
            const PassBarriers* pb = hasPrebaked ? &m_PrebakedBarriers[i] : nullptr;
            ExecutePass(cmd, *pass, pb);
        }

        // ── Finalization: transition color render targets to SHADER_READ_ONLY_OPTIMAL ──
        // ImGui samples them after Execute() completes.
        if (hasPrebaked && !m_FinalizeBarriers.empty()) {
            vkCmdPipelineBarrier(cmd,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0, 0, nullptr, 0, nullptr,
                static_cast<uint32_t>(m_FinalizeBarriers.size()),
                m_FinalizeBarriers.data());
        } else if (!hasPrebaked) {
            // Fallback: scan runtime layouts and finalize any COLOR_ATTACHMENT targets.
            std::vector<VkImageMemoryBarrier> fallbackFinalize;
            for (auto& [idx, entry] : m_RuntimeLayouts) {
                if (entry.layout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) continue;
                if (entry.image  == VK_NULL_HANDLE) continue;
                fallbackFinalize.push_back({
                    .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    .srcAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT,
                    .oldLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .image               = entry.image,
                    .subresourceRange    = { entry.aspect, 0, 1, 0, 1 },
                });
                entry.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }
            if (!fallbackFinalize.empty()) {
                vkCmdPipelineBarrier(cmd,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    0, 0, nullptr, 0, nullptr,
                    static_cast<uint32_t>(fallbackFinalize.size()),
                    fallbackFinalize.data());
            }
        }
    }

    void FrameGraphBackendVK::ExecutePass(VkCommandBuffer cmd,
                                          const CHEngine::PassDesc& pass,
                                          const PassBarriers* prebaked)
    {
        PipelineVK* pipeline = m_Factory.Pipelines.Get(pass.Pipeline);
        if (!pipeline) {
            CHE_CORE_WARN("FrameGraphBackendVK: pass '{}' has no valid pipeline, skipping",
                          pass.Name.c_str());
            return;
        }

        // ── Image barriers ────────────────────────────────────────────────────
        if (prebaked && !prebaked->pre.empty()) {
            vkCmdPipelineBarrier(cmd,
                prebaked->preSrc, prebaked->preDst,
                0, 0, nullptr, 0, nullptr,
                static_cast<uint32_t>(prebaked->pre.size()),
                prebaked->pre.data());
        } else {
            // Fallback: issue barriers inline from runtime layout state.
            // Also stores image+aspect so Execute() can finalize layouts after all passes.
            auto barrierTo = [&](CHEngine::TextureHandle th, VkImageLayout to) {
                if (!th.IsValid()) return;
                TextureVK* tex = m_Factory.Textures.Get(th);
                if (!tex) return;

                VkImage            image  = tex->GetImage();
                VkImageAspectFlags aspect = GuessAspect(tex->GetVkFormat());
                VkImageLayout      from   = VK_IMAGE_LAYOUT_UNDEFINED;

                auto it = m_RuntimeLayouts.find(th.index);
                if (it != m_RuntimeLayouts.end()) from = it->second.layout;

                m_RuntimeLayouts[th.index] = { to, image, aspect };

                if (from == to) return;
                IssueBarrier(cmd, image, from, to, aspect);
            };

            auto isAttachmentFallback = [&](CHEngine::TextureHandle th) -> bool {
                for (auto ca : pass.ColorAttachments)
                    if (ca.index == th.index) return true;
                if (pass.DepthAttachment.IsValid() && pass.DepthAttachment.index == th.index)
                    return true;
                return false;
            };

            // Color attachments → COLOR_ATTACHMENT_OPTIMAL.
            for (auto th : pass.ColorAttachments)
                barrierTo(th, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

            // Depth attachment → DEPTH_STENCIL_ATTACHMENT_OPTIMAL.
            if (pass.DepthAttachment.IsValid()) {
                TextureVK* dtex = m_Factory.Textures.Get(pass.DepthAttachment);
                const bool isDepth = dtex && (GuessAspect(dtex->GetVkFormat()) != VK_IMAGE_ASPECT_COLOR_BIT);
                barrierTo(pass.DepthAttachment,
                    isDepth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                            : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            }

            // Writes (additional write targets not listed as attachments).
            for (auto th : pass.Writes) {
                TextureVK* tex = m_Factory.Textures.Get(th);
                if (!tex) continue;
                VkImageLayout to = (GuessAspect(tex->GetVkFormat()) == VK_IMAGE_ASPECT_COLOR_BIT)
                    ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                    : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                barrierTo(th, to);
            }

            // Reads → shader-read layout (skip attachments).
            for (auto th : pass.Reads) {
                if (isAttachmentFallback(th)) continue;
                TextureVK* tex = m_Factory.Textures.Get(th);
                if (!tex) continue;
                const bool isDepth = (GuessAspect(tex->GetVkFormat()) != VK_IMAGE_ASPECT_COLOR_BIT);
                barrierTo(th, isDepth
                    ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                    : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
        }

        // ── Build VkRenderingInfo ─────────────────────────────────────────────
        // Determine color attachment: use PassDesc attachments if set, otherwise swapchain.
        const bool hasExplicitColor = !pass.ColorAttachments.empty() &&
                                       pass.ColorAttachments[0].IsValid();
        const bool hasExplicitDepth = pass.DepthAttachment.IsValid();

        VkRenderingAttachmentInfo colorAttachment = {
            .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .loadOp      = ToVkLoadOp(pass.ColorLoadOp),
            .storeOp     = ToVkStoreOp(pass.ColorStoreOp),
            .clearValue  = { .color = {{ pass.ClearColor.r, pass.ClearColor.g,
                                         pass.ClearColor.b, pass.ClearColor.a }} },
        };

        if (hasExplicitColor) {
            TextureVK* tex = m_Factory.Textures.Get(pass.ColorAttachments[0]);
            colorAttachment.imageView = tex ? tex->GetView() : VK_NULL_HANDLE;
        } else {
            // Default: render to swapchain.
            colorAttachment.imageView = m_Ctx.GetCurrentSwapchainImageView();
        }

        VkRenderingAttachmentInfo depthAttachment = {
            .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .loadOp      = ToVkLoadOp(pass.DepthLoadOp),
            .storeOp     = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .clearValue  = { .depthStencil = { pass.ClearDepth, 0 } },
        };

        VkRenderingAttachmentInfo* pDepthAttachment = nullptr;
        if (hasExplicitDepth) {
            TextureVK* depthTex = m_Factory.Textures.Get(pass.DepthAttachment);
            if (depthTex) {
                depthAttachment.imageView = depthTex->GetView();
                pDepthAttachment = &depthAttachment;
            }
        } else {
            // Use swapchain depth (from VulkanContext).
            // Access internal depth view via GetContext (if available).
            // For now skip swapchain depth attachment — subpasses can declare it explicitly.
            pDepthAttachment = nullptr;
        }

        VkExtent2D extent = m_Ctx.GetSwapchainExtent();
        if (pass.ViewportWidth > 0 && pass.ViewportHeight > 0)
            extent = { pass.ViewportWidth, pass.ViewportHeight };

        VkRenderingInfo renderingInfo = {
            .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea           = { {0, 0}, extent },
            .layerCount           = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &colorAttachment,
            .pDepthAttachment     = pDepthAttachment,
        };

        // ── Begin rendering ───────────────────────────────────────────────────
        m_Ctx.CmdBeginRendering(cmd, &renderingInfo);

        // ── Viewport / scissor ────────────────────────────────────────────────
        // Используем negative viewport height (VK_KHR_maintenance1, core c VK 1.1):
        // origin переносится в нижний-левый угол → gl_Position.y = +1 трактуется
        // как верх экрана (как в OpenGL/Metal). Это автоматически:
        //  • убирает необходимость Y-flip в clipCorr;
        //  • сохраняет нормальный winding (CCW = front) — иначе CullMode::Back
        //    отбрасывает передние грани, и в кубах видны только задние;
        //  • не трогает depth (Z всё равно [0,1]).
        VkViewport viewport = {
            .x = 0.0f, .y = static_cast<float>(extent.height),
            .width  = static_cast<float>(extent.width),
            .height = -static_cast<float>(extent.height),
            .minDepth = 0.0f, .maxDepth = 1.0f,
        };
        VkRect2D scissor = { {0, 0}, extent };
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        // ── Bind pipeline ─────────────────────────────────────────────────────
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetPipeline());

        // ── Draw ──────────────────────────────────────────────────────────────
        if (pass.Fullscreen) {
            // Per-pass uniforms/textures only.
            VkDescriptorSet set = AllocateAndWriteSet(pass, nullptr);
            if (set != VK_NULL_HANDLE)
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline->GetLayout(), 0, 1, &set, 0, nullptr);

            // Fullscreen triangle: vertex shader generates positions from gl_VertexIndex.
            vkCmdDraw(cmd, 3, 1, 0, 0);
        } else {
            for (const CHEngine::DrawDesc& draw : pass.Draws) {
                if (draw.IndexCount == 0) continue;

                BufferVK* vb = m_Factory.Buffers.Get(draw.VertexBuffer);
                BufferVK* ib = draw.IndexBuffer.IsValid()
                    ? m_Factory.Buffers.Get(draw.IndexBuffer) : nullptr;

                if (!vb) continue;

                VkBuffer     vbHandle = vb->GetBuffer();
                VkDeviceSize vbOffset = 0;
                vkCmdBindVertexBuffers(cmd, 0, 1, &vbHandle, &vbOffset);

                if (ib) {
                    VkIndexType idxType = (draw.IdxFormat == CHEngine::IndexFormat::UInt16)
                        ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
                    vkCmdBindIndexBuffer(cmd, ib->GetBuffer(), 0, idxType);
                }

                // Descriptor set covering pass + draw uniforms/textures.
                VkDescriptorSet set = AllocateAndWriteSet(pass, &draw);
                if (set != VK_NULL_HANDLE)
                    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        pipeline->GetLayout(), 0, 1, &set, 0, nullptr);

                vkCmdDrawIndexed(cmd,
                    draw.IndexCount,
                    draw.InstanceCount,
                    draw.FirstIndex,
                    draw.BaseVertex,
                    0);
            }
        }

        m_Ctx.CmdEndRendering(cmd);
    }

} // namespace CHModules
