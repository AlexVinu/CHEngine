#include "ShaderVK.h"

#include <Log/Log.h>

namespace CHModules
{
    ShaderVK::ShaderVK(const CHEngine::String& /*vertexSrc*/, const CHEngine::String& /*fragmentSrc*/)
    {
        // TODO: загрузить SPIR-V модули, создать VkShaderModule
        m_RendererID = 0;
        CHE_CORE_WARN("ShaderVK: скелетная реализация — SPIR-V загрузка пока не реализована");
    }

    ShaderVK::~ShaderVK()
    {
        // TODO: vkDestroyShaderModule
    }

    void ShaderVK::Bind() const {}
    void ShaderVK::Unbind() const {}

    bool ShaderVK::Reload(const CHEngine::String& /*vertexSrc*/, const CHEngine::String& /*fragmentSrc*/)
    {
        CHE_CORE_WARN("ShaderVK::Reload — не реализовано");
        return false;
    }

    void ShaderVK::SetInt(const CHEngine::String& /*name*/, int /*value*/) {}
    void ShaderVK::SetFloat(const CHEngine::String& /*name*/, float /*value*/) {}
    void ShaderVK::SetFloat3(const CHEngine::String& /*name*/, float /*x*/, float /*y*/, float /*z*/) {}
    void ShaderVK::SetFloat4(const CHEngine::String& /*name*/, float /*x*/, float /*y*/, float /*z*/, float /*w*/) {}
    void ShaderVK::SetMat4(const CHEngine::String& /*name*/, const float* /*matrix*/) {}
}
