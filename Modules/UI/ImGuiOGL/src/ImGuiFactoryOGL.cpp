#include "ImGuiFactoryOGL.h"

CHEngine::IImGuiLayer* CHModules::ImGuiFactoryOGL::CreateImGuiLayer(void* nativeWindow)
{
    return CreateImpl<ImGuiLayerOGL>(nativeWindow);
}

void CHModules::ImGuiFactoryOGL::Delete(CHEngine::IImGuiLayer* ptr)
{
    DestroyImpl(static_cast<ImGuiLayerOGL*>(ptr));
}

CHEngine::ModuleType CHModules::ImGuiFactoryOGL::GetType() const
{
    return CHEngine::ModuleType::ImGui;
}

IMPLEMENT_MODULE_FACTORY(CHModules::ImGuiFactoryOGL)