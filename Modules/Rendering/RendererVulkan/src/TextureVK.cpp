#include "TextureVK.h"
#include "VulkanContext.h"
#include "VulkanGlobals.h"

#include <Log/Log.h>
#include <cstring>

namespace CHModules {

    namespace {
        VulkanContext* GetCtx() {
            return static_cast<VulkanContext*>(VKGlobals::g_ContextPtr);
        }

        VkImageAspectFlags GetAspect(VkFormat fmt) {
            if (fmt == VK_FORMAT_D32_SFLOAT ||
                fmt == VK_FORMAT_D32_SFLOAT_S8_UINT ||
                fmt == VK_FORMAT_D24_UNORM_S8_UINT ||
                fmt == VK_FORMAT_D16_UNORM)
                return VK_IMAGE_ASPECT_DEPTH_BIT;
            return VK_IMAGE_ASPECT_COLOR_BIT;
        }

        void TransitionLayout(VulkanContext* ctx, VkImage image,
                              VkImageLayout from, VkImageLayout to,
                              VkImageAspectFlags aspect)
        {
            VkCommandBuffer cmd = ctx->BeginSingleTimeCommands();

            VkImageMemoryBarrier barrier = {
                .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .oldLayout           = from,
                .newLayout           = to,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image               = image,
                .subresourceRange    = { aspect, 0, 1, 0, 1 },
            };

            VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

            if (from == VK_IMAGE_LAYOUT_UNDEFINED && to == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            } else if (from == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && to == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            } else {
                barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            }

            vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
            ctx->EndSingleTimeCommands(cmd);
        }
    }

    VkFormat TextureVK::ToVkFormat(CHEngine::TextureFormat fmt)
    {
        using TF = CHEngine::TextureFormat;
        switch (fmt) {
        case TF::R8_UNORM:           return VK_FORMAT_R8_UNORM;
        case TF::R8_SNORM:           return VK_FORMAT_R8_SNORM;
        case TF::R8_UINT:            return VK_FORMAT_R8_UINT;
        case TF::R8_SINT:            return VK_FORMAT_R8_SINT;
        case TF::RG8_UNORM:          return VK_FORMAT_R8G8_UNORM;
        case TF::RGBA8_UNORM:        return VK_FORMAT_R8G8B8A8_UNORM;
        case TF::RGBA8_UNORM_SRGB:   return VK_FORMAT_R8G8B8A8_SRGB;
        case TF::BGRA8_UNORM:        return VK_FORMAT_B8G8R8A8_UNORM;
        case TF::BGRA8_UNORM_SRGB:   return VK_FORMAT_B8G8R8A8_SRGB;
        case TF::R16_FLOAT:          return VK_FORMAT_R16_SFLOAT;
        case TF::RG16_FLOAT:         return VK_FORMAT_R16G16_SFLOAT;
        case TF::RGBA16_FLOAT:       return VK_FORMAT_R16G16B16A16_SFLOAT;
        case TF::R32_FLOAT:          return VK_FORMAT_R32_SFLOAT;
        case TF::RGBA32_FLOAT:       return VK_FORMAT_R32G32B32A32_SFLOAT;
        case TF::D16_UNORM:          return VK_FORMAT_D16_UNORM;
        case TF::D24_UNORM_S8_UINT:  return VK_FORMAT_D24_UNORM_S8_UINT;
        case TF::D32_FLOAT:          return VK_FORMAT_D32_SFLOAT;
        case TF::D32_FLOAT_S8_UINT:  return VK_FORMAT_D32_SFLOAT_S8_UINT;
        case TF::BC1_RGBA_UNORM:     return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        case TF::BC3_UNORM:          return VK_FORMAT_BC3_UNORM_BLOCK;
        case TF::BC5_UNORM:          return VK_FORMAT_BC5_UNORM_BLOCK;
        case TF::BC7_UNORM:          return VK_FORMAT_BC7_UNORM_BLOCK;
        default:                     return VK_FORMAT_R8G8B8A8_UNORM;
        }
    }

    VkImageType TextureVK::ToVkImageType(CHEngine::TextureType type)
    {
        using TT = CHEngine::TextureType;
        switch (type) {
        case TT::Texture1D:
        case TT::Texture1DArray: return VK_IMAGE_TYPE_1D;
        case TT::Texture3D:      return VK_IMAGE_TYPE_3D;
        default:                 return VK_IMAGE_TYPE_2D;
        }
    }

    TextureVK::TextureVK(const uint8_t* data,
                         uint32_t width,
                         uint32_t height,
                         uint32_t channels,
                         uint32_t mipLevels,
                         uint32_t arrayLayers,
                         CHEngine::TextureFormat format,
                         CHEngine::TextureType   type,
                         CHEngine::TextureUsage  usage,
                         CHEngine::MemoryType    /*memory*/,
                         const char* /*debugName*/)
        : m_Width(width), m_Height(height)
    {
        VulkanContext* ctx = GetCtx();
        if (!ctx) { CHE_CORE_ERROR("TextureVK: no VulkanContext"); return; }

        m_VkFormat = ToVkFormat(format);
        VkImageType imgType = ToVkImageType(type);

        // Build usage flags.
        VkImageUsageFlags vkUsage = 0;
        auto u = static_cast<uint32_t>(usage);
        if (u & static_cast<uint32_t>(CHEngine::TextureUsage::ShaderResource))
            vkUsage |= VK_IMAGE_USAGE_SAMPLED_BIT;
        if (u & static_cast<uint32_t>(CHEngine::TextureUsage::RenderTarget))
            vkUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if (u & static_cast<uint32_t>(CHEngine::TextureUsage::DepthStencil))
            vkUsage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        if (u & static_cast<uint32_t>(CHEngine::TextureUsage::TransferSrc))
            vkUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        if (u & static_cast<uint32_t>(CHEngine::TextureUsage::TransferDst))
            vkUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        // If we have pixel data we need TRANSFER_DST.
        if (data) vkUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        // If no usage was set but we have data, default to sampled.
        if (vkUsage == 0 || (data && !(vkUsage & VK_IMAGE_USAGE_SAMPLED_BIT)))
            vkUsage |= VK_IMAGE_USAGE_SAMPLED_BIT;

        VkImageCreateFlags createFlags = 0;
        uint32_t layers = arrayLayers;
        if (type == CHEngine::TextureType::TextureCube) {
            createFlags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
            layers = 6;
        }

        VkImageCreateInfo imageInfo = {
            .sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .flags       = createFlags,
            .imageType   = imgType,
            .format      = m_VkFormat,
            .extent      = { std::max(width, 1u), std::max(height, 1u), 1 },
            .mipLevels   = std::max(mipLevels, 1u),
            .arrayLayers = std::max(layers, 1u),
            .samples     = VK_SAMPLE_COUNT_1_BIT,
            .tiling      = VK_IMAGE_TILING_OPTIMAL,
            .usage       = vkUsage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };

        VmaAllocationCreateInfo vmaInfo = {};
        vmaInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VkResult res = vmaCreateImage(ctx->GetAllocator(), &imageInfo, &vmaInfo,
                                      &m_Image, &m_Alloc, nullptr);
        if (res != VK_SUCCESS) {
            CHE_CORE_ERROR("TextureVK: vmaCreateImage failed");
            return;
        }

        // Upload pixel data via staging buffer.
        if (data && width > 0 && height > 0 && channels > 0) {
            const uint64_t dataSize = static_cast<uint64_t>(width) * height * channels;

            VkBufferCreateInfo stagingInfo = {
                .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size        = dataSize,
                .usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            };
            VmaAllocationCreateInfo stagingVmaInfo = {};
            stagingVmaInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

            VkBuffer stagingBuf; VmaAllocation stagingAlloc;
            vmaCreateBuffer(ctx->GetAllocator(), &stagingInfo, &stagingVmaInfo,
                            &stagingBuf, &stagingAlloc, nullptr);

            void* mapped = nullptr;
            vmaMapMemory(ctx->GetAllocator(), stagingAlloc, &mapped);
            std::memcpy(mapped, data, dataSize);
            vmaUnmapMemory(ctx->GetAllocator(), stagingAlloc);

            TransitionLayout(ctx, m_Image,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_ASPECT_COLOR_BIT);

            VkCommandBuffer cmd = ctx->BeginSingleTimeCommands();
            VkBufferImageCopy region = {
                .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
                .imageExtent      = { width, height, 1 },
            };
            vkCmdCopyBufferToImage(cmd, stagingBuf, m_Image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
            ctx->EndSingleTimeCommands(cmd);

            TransitionLayout(ctx, m_Image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_IMAGE_ASPECT_COLOR_BIT);

            m_CurrentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            vmaDestroyBuffer(ctx->GetAllocator(), stagingBuf, stagingAlloc);
        }

        // Create image view.
        VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;
        if (imgType == VK_IMAGE_TYPE_1D) viewType = VK_IMAGE_VIEW_TYPE_1D;
        else if (type == CHEngine::TextureType::TextureCube) viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        else if (type == CHEngine::TextureType::Texture2DArray) viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;

        VkImageAspectFlags aspect = GetAspect(m_VkFormat);

        VkImageViewCreateInfo viewInfo = {
            .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image    = m_Image,
            .viewType = viewType,
            .format   = m_VkFormat,
            .subresourceRange = {
                .aspectMask     = aspect,
                .baseMipLevel   = 0, .levelCount   = std::max(mipLevels, 1u),
                .baseArrayLayer = 0, .layerCount   = std::max(layers, 1u),
            },
        };
        vkCreateImageView(ctx->GetDevice(), &viewInfo, nullptr, &m_ImageView);

        // Create sampler.
        VkSamplerCreateInfo samplerInfo = {
            .sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter    = VK_FILTER_LINEAR,
            .minFilter    = VK_FILTER_LINEAR,
            .mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .mipLodBias   = 0.0f,
            .anisotropyEnable = VK_FALSE,
            .compareEnable    = VK_FALSE,
            .minLod = 0.0f, .maxLod = static_cast<float>(std::max(mipLevels, 1u)),
            .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
        };
        vkCreateSampler(ctx->GetDevice(), &samplerInfo, nullptr, &m_Sampler);
    }

    TextureVK::~TextureVK()
    {
        VulkanContext* ctx = GetCtx();
        if (!ctx) return;

        // If this texture was ever registered with ImGui (via ImGui_ImplVulkan_AddTexture),
        // release the descriptor set before destroying the sampler/view it references.
        if (m_ImGuiDescSet != VK_NULL_HANDLE) {
            auto& vkState = VKGlobals::g_SharedContext;
            if (vkState.UnregisterImGuiTexture)
                vkState.UnregisterImGuiTexture(reinterpret_cast<uint64_t>(m_ImGuiDescSet));
            m_ImGuiDescSet = VK_NULL_HANDLE;
        }

        if (m_Sampler   != VK_NULL_HANDLE) vkDestroySampler(ctx->GetDevice(), m_Sampler, nullptr);
        if (m_ImageView != VK_NULL_HANDLE) vkDestroyImageView(ctx->GetDevice(), m_ImageView, nullptr);
        if (m_Image != VK_NULL_HANDLE && m_Alloc != VK_NULL_HANDLE)
            vmaDestroyImage(ctx->GetAllocator(), m_Image, m_Alloc);
        m_Sampler   = VK_NULL_HANDLE;
        m_ImageView = VK_NULL_HANDLE;
        m_Image     = VK_NULL_HANDLE;
        m_Alloc     = VK_NULL_HANDLE;
    }

} // namespace CHModules
