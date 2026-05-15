#include "ShaderVK.h"
#include "VulkanContext.h"
#include "VulkanGlobals.h"

#include <Log/Log.h>
#include <SlangBackend/SlangBackend.h>

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
                CHE_CORE_ERROR("ShaderVK: SPIR-V blob for {0} has bad size {1}", stage, spirv.size());
                return VK_NULL_HANDLE;
            }

            VkShaderModuleCreateInfo info{};
            info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            info.codeSize = spirv.size();
            info.pCode    = reinterpret_cast<const uint32_t*>(spirv.data());

            VkShaderModule mod = VK_NULL_HANDLE;
            if (vkCreateShaderModule(device, &info, nullptr, &mod) != VK_SUCCESS)
            {
                CHE_CORE_ERROR("ShaderVK: vkCreateShaderModule failed for {0}", stage);
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

        SlangBackend* backend = SlangBackend::GetForApi(CHEngine::ERenderAPI::VULKAN);
        if (!backend)
        {
            CHE_CORE_ERROR("ShaderVK: SlangBackend unavailable for Vulkan");
            return false;
        }

        CompiledShader compiled = backend->Compile(slangSource, vertEntry, fragEntry, sourcePath);
        if (!compiled.valid)
        {
            CHE_CORE_ERROR("ShaderVK: Slang compilation failed:\n{0}", compiled.errorLog);
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
        return true;
    }

    void ShaderVK::DestroyModules()
    {
        VkDevice device = GetActiveDevice();
        if (device == VK_NULL_HANDLE)
            return;

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
                       const String& sourcePath)
    {
        if (!BuildModules(slangSource, vertEntry, fragEntry, sourcePath))
            CHE_CORE_WARN("ShaderVK: shader was created without valid modules");
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

    void ShaderVK::SetUniformBlock(CHEngine::EUniformBlock /*block*/, const void* /*data*/, uint32_t /*size*/) {}
    void ShaderVK::SetInt(const String& /*name*/, int /*value*/) {}
}
