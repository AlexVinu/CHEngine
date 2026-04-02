#pragma once

#include <Render/IShader.h>
#include <CheStl/String.h>

namespace CHModules
{
    // Vulkan шейдер — работает с SPIR-V байткодом.
    // vertexSrc/fragmentSrc интерпретируются как пути к .spv файлам.
    class ShaderVK : public CHEngine::IShader
    {
    public:
        ShaderVK(const CHEngine::String& vertexSrc, const CHEngine::String& fragmentSrc);
        ~ShaderVK() override;

        void Bind() const override;
        void Unbind() const override;
        bool Reload(const CHEngine::String& vertexSrc, const CHEngine::String& fragmentSrc) override;

        void SetUniformBlock(CHEngine::EUniformBlock block, const void* data, uint32_t size) override;
        void SetInt(const CHEngine::String& name, int value) override;
    };
}
