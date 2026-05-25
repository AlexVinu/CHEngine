#pragma once

#include <CheStl/String.h>
#include <SlangBackend/SlangBackend.h>

#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <cstdint>

namespace CHModules
{
    // Vulkan shader: compiles .slang → SPIR-V via SlangBackend, creates VkShaderModules.
    // Stores Slang reflection data (binding layout) for use by PipelineVK.
    class ShaderVK
    {
    public:
        ShaderVK(const String& slangSource,
                 const String& vertEntry,
                 const String& fragEntry,
                 const String& sourcePath,
                 SlangBackend& slang);
        ~ShaderVK();

        ShaderVK(const ShaderVK&)            = delete;
        ShaderVK& operator=(const ShaderVK&) = delete;

        bool Reload(const String& slangSource,
                    const String& vertEntry  = String("vertMain"),
                    const String& fragEntry  = String("fragMain"),
                    const String& sourcePath = String());

        VkShaderModule GetVertexModule()   const { return m_VertModule; }
        VkShaderModule GetFragmentModule() const { return m_FragModule; }

        // Reflection: bindings sorted into UBOs and samplers by VK binding number.
        struct BindingEntry { uint32_t vkBinding = 0; uint32_t set = 0; };
        const std::vector<BindingEntry>& GetUboBindings()     const { return m_UboBindings; }
        const std::vector<BindingEntry>& GetSamplerBindings() const { return m_SamplerBindings; }

    private:
        SlangBackend* m_Slang = nullptr;

        bool BuildModules(const String& slangSource,
                          const String& vertEntry,
                          const String& fragEntry,
                          const String& sourcePath);
        void DestroyModules();

        VkShaderModule m_VertModule = VK_NULL_HANDLE;
        VkShaderModule m_FragModule = VK_NULL_HANDLE;

        // Sorted lists of reflection bindings: index in vector = slot index used by FG backend.
        // UBOs are sorted ascending by vkBinding; same for samplers.
        std::vector<BindingEntry> m_UboBindings;
        std::vector<BindingEntry> m_SamplerBindings;
    };
}
