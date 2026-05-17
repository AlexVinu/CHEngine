#pragma once

#include <Render/IShader.h>
#include <CheStl/String.h>
#include <SlangBackend/SlangBackend.h>

#include <vulkan/vulkan.h>

namespace CHModules
{
    // Vulkan shader: takes a .slang source, compiles via SlangBackend → SPIR-V,
    // and creates one VkShaderModule per stage. Pipeline creation (which needs
    // these modules) happens elsewhere.
    class ShaderVK : public CHEngine::IShader
    {
    public:
        // slang: reference to the factory-owned SlangBackend (must outlive this shader)
        ShaderVK(const String& slangSource,
                 const String& vertEntry,
                 const String& fragEntry,
                 const String& sourcePath,
                 SlangBackend& slang);
        ~ShaderVK() override;

        bool Reload(const String& slangSource,
                    const String& vertEntry  = String("vertMain"),
                    const String& fragEntry  = String("fragMain"),
                    const String& sourcePath = String()) override;

        void SetUniformBlock(CHEngine::EUniformBlock block, const void* data, uint32_t size) override;
        void SetInt(const String& name, int value) override;

        VkShaderModule GetVertexModule()   const { return m_VertModule; }
        VkShaderModule GetFragmentModule() const { return m_FragModule; }

    private:
        SlangBackend* m_Slang = nullptr; // owned by factory; valid for shader lifetime
        bool BuildModules(const String& slangSource,
                          const String& vertEntry,
                          const String& fragEntry,
                          const String& sourcePath);
        void DestroyModules();

        VkShaderModule m_VertModule = VK_NULL_HANDLE;
        VkShaderModule m_FragModule = VK_NULL_HANDLE;
    };
}
