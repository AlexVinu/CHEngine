#include "PipelineVK.h"
#include "RenderFactoryVK.h"
#include "ShaderVK.h"
#include "TextureVK.h"

#include <Log/Log.h>
#include <algorithm>
#include <vector>

namespace CHModules {

    PipelineVK::PipelineVK(const CHEngine::PipelineDesc& desc, RenderFactoryVK& factory)
        : m_Desc(desc)
    {
        if (!Build(desc, factory))
            CHE_CORE_ERROR("PipelineVK: failed to build pipeline for shader {}",
                           desc.Shader.index);
    }

    PipelineVK::~PipelineVK()
    {
        Destroy();
    }

    void PipelineVK::Destroy()
    {
        if (m_Pipeline       != VK_NULL_HANDLE) { vkDestroyPipeline(m_Device, m_Pipeline, nullptr);       m_Pipeline = VK_NULL_HANDLE; }
        if (m_PipelineLayout != VK_NULL_HANDLE) { vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr); m_PipelineLayout = VK_NULL_HANDLE; }
        if (m_SetLayout      != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(m_Device, m_SetLayout, nullptr); m_SetLayout = VK_NULL_HANDLE; }
    }

    // ─── Enum converters ──────────────────────────────────────────────────────

    VkFormat PipelineVK::ToVkVertexFormat(CHEngine::VertexFormat fmt)
    {
        using VF = CHEngine::VertexFormat;
        switch (fmt) {
        case VF::Float:        return VK_FORMAT_R32_SFLOAT;
        case VF::Float2:       return VK_FORMAT_R32G32_SFLOAT;
        case VF::Float3:       return VK_FORMAT_R32G32B32_SFLOAT;
        case VF::Float4:       return VK_FORMAT_R32G32B32A32_SFLOAT;
        case VF::Int:          return VK_FORMAT_R32_SINT;
        case VF::Int2:         return VK_FORMAT_R32G32_SINT;
        case VF::Int3:         return VK_FORMAT_R32G32B32_SINT;
        case VF::Int4:         return VK_FORMAT_R32G32B32A32_SINT;
        case VF::UInt:         return VK_FORMAT_R32_UINT;
        case VF::UInt2:        return VK_FORMAT_R32G32_UINT;
        case VF::UInt3:        return VK_FORMAT_R32G32B32_UINT;
        case VF::UInt4:        return VK_FORMAT_R32G32B32A32_UINT;
        case VF::UByte4:       return VK_FORMAT_R8G8B8A8_UINT;
        case VF::UByte4_Norm:  return VK_FORMAT_R8G8B8A8_UNORM;
        default:               return VK_FORMAT_R32G32B32_SFLOAT;
        }
    }

    VkCompareOp PipelineVK::ToVkCompareOp(CHEngine::CompareOp op)
    {
        using CO = CHEngine::CompareOp;
        switch (op) {
        case CO::Never:        return VK_COMPARE_OP_NEVER;
        case CO::Less:         return VK_COMPARE_OP_LESS;
        case CO::Equal:        return VK_COMPARE_OP_EQUAL;
        case CO::LessEqual:    return VK_COMPARE_OP_LESS_OR_EQUAL;
        case CO::Greater:      return VK_COMPARE_OP_GREATER;
        case CO::NotEqual:     return VK_COMPARE_OP_NOT_EQUAL;
        case CO::GreaterEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case CO::Always:       return VK_COMPARE_OP_ALWAYS;
        }
        return VK_COMPARE_OP_LESS;
    }

    VkBlendFactor PipelineVK::ToVkBlendFactor(CHEngine::BlendFactor f)
    {
        using BF = CHEngine::BlendFactor;
        switch (f) {
        case BF::Zero:                return VK_BLEND_FACTOR_ZERO;
        case BF::One:                 return VK_BLEND_FACTOR_ONE;
        case BF::SrcColor:            return VK_BLEND_FACTOR_SRC_COLOR;
        case BF::OneMinusSrcColor:    return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case BF::DstColor:            return VK_BLEND_FACTOR_DST_COLOR;
        case BF::OneMinusDstColor:    return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        case BF::SrcAlpha:            return VK_BLEND_FACTOR_SRC_ALPHA;
        case BF::OneMinusSrcAlpha:    return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case BF::DstAlpha:            return VK_BLEND_FACTOR_DST_ALPHA;
        case BF::OneMinusDstAlpha:    return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case BF::ConstantColor:       return VK_BLEND_FACTOR_CONSTANT_COLOR;
        case BF::OneMinusConstantColor: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
        case BF::SrcAlphaSaturate:    return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
        default:                      return VK_BLEND_FACTOR_ONE;
        }
    }

    VkBlendOp PipelineVK::ToVkBlendOp(CHEngine::BlendOp op)
    {
        using BO = CHEngine::BlendOp;
        switch (op) {
        case BO::Add:             return VK_BLEND_OP_ADD;
        case BO::Subtract:        return VK_BLEND_OP_SUBTRACT;
        case BO::ReverseSubtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
        case BO::Min:             return VK_BLEND_OP_MIN;
        case BO::Max:             return VK_BLEND_OP_MAX;
        }
        return VK_BLEND_OP_ADD;
    }

    VkPrimitiveTopology PipelineVK::ToVkTopology(CHEngine::PrimitiveType p)
    {
        using PT = CHEngine::PrimitiveType;
        switch (p) {
        case PT::Points:        return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case PT::Lines:         return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case PT::LineStrip:     return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case PT::Triangles:     return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case PT::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case PT::TriangleFan:   return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
        default:                return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        }
    }

    VkCullModeFlags PipelineVK::ToVkCullMode(CHEngine::CullMode c)
    {
        using CM = CHEngine::CullMode;
        switch (c) {
        case CM::None:  return VK_CULL_MODE_NONE;
        case CM::Front: return VK_CULL_MODE_FRONT_BIT;
        case CM::Back:  return VK_CULL_MODE_BACK_BIT;
        }
        return VK_CULL_MODE_BACK_BIT;
    }

    VkFrontFace PipelineVK::ToVkFrontFace(CHEngine::FrontFace f)
    {
        using FF = CHEngine::FrontFace;
        // Negative viewport height (см. FrameGraphBackendVK) делает winding
        // в framebuffer coords совпадающим с GL-конвенцией. Прямой маппинг:
        // CCW в GL = CCW в Vulkan framebuffer. Раньше клавиатура свопалась
        // для компенсации Y-flip в clipCorr, который убран.
        return (f == FF::CCW) ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
    }

    // ─── Build ────────────────────────────────────────────────────────────────

    bool PipelineVK::Build(const CHEngine::PipelineDesc& desc, RenderFactoryVK& factory)
    {
        m_Device = factory.GetDevice();
        if (m_Device == VK_NULL_HANDLE) return false;

        // ── Shader modules ─────────────────────────────────────────────────────
        ShaderVK* shader = factory.Shaders.Get(desc.Shader);
        if (!shader) {
            CHE_CORE_ERROR("PipelineVK: invalid ShaderHandle");
            return false;
        }

        // ── Build descriptor set layout from reflection ─────────────────────────
        {
            const auto& uboBindings     = shader->GetUboBindings();
            const auto& samplerBindings = shader->GetSamplerBindings();

            m_DescLayout.uboVkBindings.resize(uboBindings.size());
            for (size_t i = 0; i < uboBindings.size(); ++i)
                m_DescLayout.uboVkBindings[i] = uboBindings[i].vkBinding;

            m_DescLayout.samplerVkBindings.resize(samplerBindings.size());
            for (size_t i = 0; i < samplerBindings.size(); ++i)
                m_DescLayout.samplerVkBindings[i] = samplerBindings[i].vkBinding;

            std::vector<VkDescriptorSetLayoutBinding> bindings;
            bindings.reserve(uboBindings.size() + samplerBindings.size());

            for (const auto& e : uboBindings)
                bindings.push_back({
                    .binding         = e.vkBinding,
                    .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .descriptorCount = 1,
                    .stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                });

            for (const auto& e : samplerBindings)
                bindings.push_back({
                    .binding         = e.vkBinding,
                    .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount = 1,
                    .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT,
                });

            // Use PARTIALLY_BOUND so passes don't need to write all bindings.
            std::vector<VkDescriptorBindingFlags> bindingFlags(bindings.size(),
                VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);

            VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo = {
                .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                .bindingCount  = static_cast<uint32_t>(bindingFlags.size()),
                .pBindingFlags = bindingFlags.data(),
            };

            VkDescriptorSetLayoutCreateInfo setLayoutInfo = {
                .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pNext        = &flagsInfo,
                .bindingCount = static_cast<uint32_t>(bindings.size()),
                .pBindings    = bindings.data(),
            };

            if (vkCreateDescriptorSetLayout(m_Device, &setLayoutInfo, nullptr, &m_SetLayout) != VK_SUCCESS) {
                CHE_CORE_ERROR("PipelineVK: failed to create descriptor set layout");
                return false;
            }
        }

        // ── Pipeline layout ─────────────────────────────────────────────────────
        {
            VkPipelineLayoutCreateInfo layoutInfo = {
                .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .setLayoutCount = 1,
                .pSetLayouts    = &m_SetLayout,
            };
            if (vkCreatePipelineLayout(m_Device, &layoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS) {
                CHE_CORE_ERROR("PipelineVK: failed to create pipeline layout");
                return false;
            }
        }

        // ── Vertex input ────────────────────────────────────────────────────────
        const CHEngine::VertexInputLayout& vtxLayout = desc.VertexLayout;

        std::vector<VkVertexInputBindingDescription> bindingDescs;
        for (uint32_t i = 0; i < static_cast<uint32_t>(vtxLayout.Strides.size()); ++i)
            bindingDescs.push_back({ i, vtxLayout.Strides[i], VK_VERTEX_INPUT_RATE_VERTEX });

        std::vector<VkVertexInputAttributeDescription> attrDescs;
        for (uint32_t i = 0; i < static_cast<uint32_t>(vtxLayout.Attributes.size()); ++i) {
            const auto& a = vtxLayout.Attributes[i];
            attrDescs.push_back({ i, a.Slot, ToVkVertexFormat(a.Format), a.Offset });
        }

        VkPipelineVertexInputStateCreateInfo vertexInput = {
            .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount   = static_cast<uint32_t>(bindingDescs.size()),
            .pVertexBindingDescriptions      = bindingDescs.data(),
            .vertexAttributeDescriptionCount = static_cast<uint32_t>(attrDescs.size()),
            .pVertexAttributeDescriptions    = attrDescs.data(),
        };

        // ── Shader stages ───────────────────────────────────────────────────────
        VkPipelineShaderStageCreateInfo stages[2] = {
            {
                .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage  = VK_SHADER_STAGE_VERTEX_BIT,
                .module = shader->GetVertexModule(),
                .pName  = "main",
            },
            {
                .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = shader->GetFragmentModule(),
                .pName  = "main",
            },
        };

        // ── Input assembly ──────────────────────────────────────────────────────
        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
            .sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = ToVkTopology(desc.Primitive),
        };

        // ── Dynamic state (viewport + scissor set at draw time) ─────────────────
        VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamicState = {
            .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = 2,
            .pDynamicStates    = dynamicStates,
        };
        VkPipelineViewportStateCreateInfo viewportState = {
            .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount  = 1,
        };

        // ── Rasterizer ──────────────────────────────────────────────────────────
        VkPipelineRasterizationStateCreateInfo raster = {
            .sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .polygonMode = desc.Raster.Wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL,
            .cullMode    = ToVkCullMode(desc.Raster.Cull),
            .frontFace   = ToVkFrontFace(desc.Raster.Face),
            .lineWidth   = 1.0f,
        };

        // ── Multisampling ───────────────────────────────────────────────────────
        VkPipelineMultisampleStateCreateInfo msaa = {
            .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        };

        // ── Depth / stencil ─────────────────────────────────────────────────────
        VkPipelineDepthStencilStateCreateInfo depthStencil = {
            .sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable  = desc.Depth.Test  ? VK_TRUE : VK_FALSE,
            .depthWriteEnable = desc.Depth.Write ? VK_TRUE : VK_FALSE,
            .depthCompareOp   = ToVkCompareOp(desc.Depth.Compare),
        };

        // ── Color blend ─────────────────────────────────────────────────────────
        VkPipelineColorBlendAttachmentState blendAttach = {
            .blendEnable         = desc.Blend.Enable ? VK_TRUE : VK_FALSE,
            .srcColorBlendFactor = ToVkBlendFactor(desc.Blend.SrcColor),
            .dstColorBlendFactor = ToVkBlendFactor(desc.Blend.DstColor),
            .colorBlendOp        = ToVkBlendOp(desc.Blend.ColorOp),
            .srcAlphaBlendFactor = ToVkBlendFactor(desc.Blend.SrcAlpha),
            .dstAlphaBlendFactor = ToVkBlendFactor(desc.Blend.DstAlpha),
            .alphaBlendOp        = ToVkBlendOp(desc.Blend.AlphaOp),
            .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                   VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        };
        VkPipelineColorBlendStateCreateInfo colorBlend = {
            .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments    = &blendAttach,
        };

        // ── Dynamic rendering info (no render pass objects) ─────────────────────
        std::vector<VkFormat> colorFmts;
        for (auto fmt : desc.ColorFormats)
            colorFmts.push_back(TextureVK::ToVkFormat(fmt));

        // Default color format if not specified.
        if (colorFmts.empty()) {
            VulkanContext* ctx = factory.GetContext();
            colorFmts.push_back(ctx ? ctx->GetSwapchainFormat() : VK_FORMAT_B8G8R8A8_SRGB);
        }

        // Use the format from PipelineDesc — it must match the actual depth attachment texture.
        VkFormat depthFmt = TextureVK::ToVkFormat(desc.DepthFormat);

        VkPipelineRenderingCreateInfo renderingInfo = {
            .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .colorAttachmentCount    = static_cast<uint32_t>(colorFmts.size()),
            .pColorAttachmentFormats = colorFmts.data(),
            .depthAttachmentFormat   = depthFmt,
        };

        // ── Final pipeline ──────────────────────────────────────────────────────
        VkGraphicsPipelineCreateInfo pipelineInfo = {
            .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext               = &renderingInfo,
            .stageCount          = 2,
            .pStages             = stages,
            .pVertexInputState   = &vertexInput,
            .pInputAssemblyState = &inputAssembly,
            .pViewportState      = &viewportState,
            .pRasterizationState = &raster,
            .pMultisampleState   = &msaa,
            .pDepthStencilState  = &depthStencil,
            .pColorBlendState    = &colorBlend,
            .pDynamicState       = &dynamicState,
            .layout              = m_PipelineLayout,
        };

        VkResult res = vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline);
        if (res != VK_SUCCESS) {
            CHE_CORE_ERROR("PipelineVK: vkCreateGraphicsPipelines failed");
            return false;
        }

        return true;
    }

} // namespace CHModules
