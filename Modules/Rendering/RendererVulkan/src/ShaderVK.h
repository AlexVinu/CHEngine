#pragma once

#include <Render/IShader.h>
#include <Containers/String.h>

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

        void SetInt   (const CHEngine::String& name, int value) override;
        void SetFloat (const CHEngine::String& name, float value) override;
        void SetFloat3(const CHEngine::String& name, float x, float y, float z) override;
        void SetFloat4(const CHEngine::String& name, float x, float y, float z, float w) override;
        void SetMat4  (const CHEngine::String& name, const float* matrix) override;
    };
}
