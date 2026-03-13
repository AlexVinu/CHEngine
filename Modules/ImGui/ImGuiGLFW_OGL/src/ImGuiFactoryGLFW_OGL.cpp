#include "ImGuiFactoryGLFW_OGL.h"

inline CHEngine::IImGuiLayer* CHModules::ImGuiFactoryGLFW_OGL::Create(void* nativeWindow)
{
    return CreateImpl<ImGuiLayerGLFW_OGL>(nativeWindow);
}

inline void CHModules::ImGuiFactoryGLFW_OGL::Delete(CHEngine::IImGuiLayer* layer)
{
    DestroyImpl(static_cast<ImGuiLayerGLFW_OGL*>(layer));
}

inline CHEngine::ModuleType CHModules::ImGuiFactoryGLFW_OGL::GetType() const
{
    return CHEngine::ModuleType::ImGui;
}

IMPLEMENT_MODULE_FACTORY(CHModules::ImGuiFactoryGLFW_OGL);