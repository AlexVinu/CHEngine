#include <chepch.h>
#include "ImGuiFactoryOGL.h"

CHEngine::IImGuiLayer* CHModules::ImGuiFactoryOGL::CreateImGuiLayer(CHEngine::IWindow* window)
{
    return CreateImpl<ImGuiLayerOGL>(window);
}

void CHModules::ImGuiFactoryOGL::Delete(CHEngine::IImGuiLayer* ptr)
{
    if (!ptr) return;
    DestroyImpl(static_cast<ImGuiLayerOGL*>(ptr));
}

CHEngine::ModuleType CHModules::ImGuiFactoryOGL::GetType() const
{
    return CHEngine::ModuleType::ImGui;
}

IMPLEMENT_MODULE_FACTORY(CHModules::ImGuiFactoryOGL)
