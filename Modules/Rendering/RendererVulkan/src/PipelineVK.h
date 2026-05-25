#pragma once

#include "Render/Descriptors.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>

namespace CHModules {

    struct RenderFactoryVK;

    // Per-slot mapping from UniformBinding/TextureBinding slot index → VK binding number.
    struct DescriptorLayout {
        std::vector<uint32_t> uboVkBindings;     // indexed by UniformBinding.Slot
        std::vector<uint32_t> samplerVkBindings; // indexed by TextureBinding.Slot
    };

    /// Vulkan graphics pipeline built from PipelineDesc.
    /// Holds VkPipeline, VkPipelineLayout, VkDescriptorSetLayout derived from Slang reflection.
    class PipelineVK
    {
    public:
        explicit PipelineVK(const CHEngine::PipelineDesc& desc, RenderFactoryVK& factory);
        ~PipelineVK();

        PipelineVK(const PipelineVK&)            = delete;
        PipelineVK& operator=(const PipelineVK&) = delete;

        VkPipeline            GetPipeline()     const { return m_Pipeline; }
        VkPipelineLayout      GetLayout()       const { return m_PipelineLayout; }
        VkDescriptorSetLayout GetSetLayout()    const { return m_SetLayout; }
        const DescriptorLayout& GetDescLayout() const { return m_DescLayout; }
        const CHEngine::PipelineDesc& GetDesc() const { return m_Desc; }

        CHEngine::PrimitiveType GetPrimitive() const { return m_Desc.Primitive; }

    private:
        bool Build(const CHEngine::PipelineDesc& desc, RenderFactoryVK& factory);
        void Destroy();

        static VkFormat    ToVkVertexFormat(CHEngine::VertexFormat fmt);
        static VkCompareOp ToVkCompareOp(CHEngine::CompareOp op);
        static VkBlendFactor ToVkBlendFactor(CHEngine::BlendFactor f);
        static VkBlendOp     ToVkBlendOp(CHEngine::BlendOp op);
        static VkPrimitiveTopology ToVkTopology(CHEngine::PrimitiveType p);
        static VkCullModeFlags     ToVkCullMode(CHEngine::CullMode c);
        static VkFrontFace         ToVkFrontFace(CHEngine::FrontFace f);
        static VkFormat            ToVkAttachmentFormat(CHEngine::TextureFormat f);

        CHEngine::PipelineDesc m_Desc;
        VkDevice               m_Device      = VK_NULL_HANDLE;
        VkPipeline             m_Pipeline    = VK_NULL_HANDLE;
        VkPipelineLayout       m_PipelineLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout  m_SetLayout   = VK_NULL_HANDLE;
        DescriptorLayout       m_DescLayout;
    };

} // namespace CHModules
