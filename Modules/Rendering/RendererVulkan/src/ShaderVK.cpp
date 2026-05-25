#include "ShaderVK.h"
#include "VulkanContext.h"
#include "VulkanGlobals.h"

#include <Log/Log.h>
#include <algorithm>

namespace CHModules
{
    namespace
    {
        VkDevice GetActiveDevice()
        {
            auto* ctx = static_cast<VulkanContext*>(VKGlobals::g_ContextPtr);
            return ctx ? ctx->GetDevice() : VK_NULL_HANDLE;
        }

        VkShaderModule MakeModule(VkDevice device, const std::vector<uint8_t>& spirv, const char* stage)
        {
            if (spirv.empty() || (spirv.size() % 4) != 0)
            {
                CHE_CORE_ERROR("ShaderVK: SPIR-V blob for {} has bad size {}", stage, spirv.size());
                return VK_NULL_HANDLE;
            }

            VkShaderModuleCreateInfo info{};
            info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            info.codeSize = spirv.size();
            info.pCode    = reinterpret_cast<const uint32_t*>(spirv.data());

            VkShaderModule mod = VK_NULL_HANDLE;
            if (vkCreateShaderModule(device, &info, nullptr, &mod) != VK_SUCCESS)
            {
                CHE_CORE_ERROR("ShaderVK: vkCreateShaderModule failed for {}", stage);
                return VK_NULL_HANDLE;
            }
            return mod;
        }
    }

    bool ShaderVK::BuildModules(const String& slangSource,
                                const String& vertEntry,
                                const String& fragEntry,
                                const String& sourcePath)
    {
        VkDevice device = GetActiveDevice();
        if (device == VK_NULL_HANDLE)
        {
            CHE_CORE_ERROR("ShaderVK: VulkanContext is not initialized");
            return false;
        }

        if (!m_Slang || !m_Slang->IsInitialised())
        {
            CHE_CORE_ERROR("ShaderVK: SlangBackend not initialised");
            return false;
        }

        CompiledShader compiled = m_Slang->Compile(slangSource, vertEntry, fragEntry, sourcePath);
        if (!compiled.valid)
        {
            CHE_CORE_ERROR("ShaderVK: Slang compilation failed:\n{}", compiled.errorLog);
            return false;
        }

        VkShaderModule vert = MakeModule(device, compiled.vertexCode,   "vertex");
        VkShaderModule frag = MakeModule(device, compiled.fragmentCode, "fragment");
        if (vert == VK_NULL_HANDLE || frag == VK_NULL_HANDLE)
        {
            if (vert != VK_NULL_HANDLE) vkDestroyShaderModule(device, vert, nullptr);
            if (frag != VK_NULL_HANDLE) vkDestroyShaderModule(device, frag, nullptr);
            return false;
        }

        DestroyModules();
        m_VertModule = vert;
        m_FragModule = frag;

        // Build sorted binding lists from reflection.
        m_UboBindings.clear();
        m_SamplerBindings.clear();

        for (const auto& [name, info] : compiled.params)
        {
            BindingEntry e{ info.binding, info.set };
            if (info.isSampler)
                m_SamplerBindings.push_back(e);
            else
                m_UboBindings.push_back(e);
        }

        auto byBinding = [](const BindingEntry& a, const BindingEntry& b) {
            return a.vkBinding < b.vkBinding;
        };
        auto sameBinding = [](const BindingEntry& a, const BindingEntry& b) {
            return a.vkBinding == b.vkBinding;
        };
        std::sort(m_UboBindings.begin(),     m_UboBindings.end(),     byBinding);
        std::sort(m_SamplerBindings.begin(), m_SamplerBindings.end(), byBinding);

        // Remove duplicate binding indices.  [[vk::combinedImageSampler]] causes
        // Slang reflection to emit both the Texture2D and SamplerState at the same
        // binding number; we only need one descriptor layout entry per binding.
        m_UboBindings.erase(
            std::unique(m_UboBindings.begin(),     m_UboBindings.end(),     sameBinding),
            m_UboBindings.end());
        m_SamplerBindings.erase(
            std::unique(m_SamplerBindings.begin(), m_SamplerBindings.end(), sameBinding),
            m_SamplerBindings.end());

        return true;
    }

    void ShaderVK::DestroyModules()
    {
        VkDevice device = GetActiveDevice();
        if (device == VK_NULL_HANDLE) return;

        if (m_VertModule != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(device, m_VertModule, nullptr);
            m_VertModule = VK_NULL_HANDLE;
        }
        if (m_FragModule != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(device, m_FragModule, nullptr);
            m_FragModule = VK_NULL_HANDLE;
        }
    }

    ShaderVK::ShaderVK(const String& slangSource,
                       const String& vertEntry,
                       const String& fragEntry,
                       const String& sourcePath,
                       SlangBackend& slang)
        : m_Slang(&slang)
    {
        if (!BuildModules(slangSource, vertEntry, fragEntry, sourcePath))
            CHE_CORE_WARN("ShaderVK: shader created without valid modules");
    }

    ShaderVK::~ShaderVK()
    {
        DestroyModules();
    }

    bool ShaderVK::Reload(const String& slangSource,
                          const String& vertEntry,
                          const String& fragEntry,
                          const String& sourcePath)
    {
        return BuildModules(slangSource, vertEntry, fragEntry, sourcePath);
    }

}
