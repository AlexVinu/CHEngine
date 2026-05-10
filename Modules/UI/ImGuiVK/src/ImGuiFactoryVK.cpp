#include "chepch.h"
#include "ImGuiFactoryVK.h"

CHEngine::IImGuiLayer* CHModules::ImGuiFactoryVK::CreateImGuiLayer(CHEngine::IWindow* window)
{
    return CreateImpl<ImGuiLayerVK>(window);
}

void CHModules::ImGuiFactoryVK::Delete(CHEngine::IImGuiLayer* ptr)
{
    if (!ptr) return;
    DestroyImpl(static_cast<ImGuiLayerVK*>(ptr));
}

CHEngine::ModuleType CHModules::ImGuiFactoryVK::GetType() const
{
    return CHEngine::ModuleType::ImGui;
}

IMPLEMENT_MODULE_FACTORY(CHModules::ImGuiFactoryVK)
