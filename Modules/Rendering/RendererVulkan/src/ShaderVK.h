#pragma once

#include <Render/IShader.h>
#include <CheStl/String.h>

#include <vulkan/vulkan.h>

namespace CHModules
{
    // Vulkan shader: takes a .slang source, compiles via SlangBackend → SPIR-V,
    // and creates one VkShaderModule per stage. Pipeline creation (which needs
    // these modules) happens elsewhere.
    class ShaderVK : public CHEngine::IShader
    {
    public:
        ShaderVK(const CHEngine::String& slangSource,
                 const CHEngine::String& vertEntry,
                 const CHEngine::String& fragEntry,
                 const CHEngine::String& sourcePath = CHEngine::String());
        ~ShaderVK() override;

        void Bind() const override;
        void Unbind() const override;
        bool Reload(const CHEngine::String& slangSource,
                    const CHEngine::String& vertEntry  = CHEngine::String("vertMain"),
                    const CHEngine::String& fragEntry  = CHEngine::String("fragMain"),
                    const CHEngine::String& sourcePath = CHEngine::String()) override;

        void SetUniformBlock(CHEngine::EUniformBlock block, const void* data, uint32_t size) override;
        void SetInt(const CHEngine::String& name, int value) override;

        VkShaderModule GetVertexModule()   const { return m_VertModule; }
        VkShaderModule GetFragmentModule() const { return m_FragModule; }

    private:
        bool BuildModules(const CHEngine::String& slangSource,
                          const CHEngine::String& vertEntry,
                          const CHEngine::String& fragEntry,
                          const CHEngine::String& sourcePath);
        void DestroyModules();

        VkShaderModule m_VertModule = VK_NULL_HANDLE;
        VkShaderModule m_FragModule = VK_NULL_HANDLE;
    };
}
