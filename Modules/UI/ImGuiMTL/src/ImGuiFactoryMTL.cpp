#include "chepch.h"
#include "ImGuiFactoryMTL.h"

CHEngine::IImGuiLayer* CHModules::ImGuiFactoryMTL::CreateImGuiLayer(
    CHEngine::IWindow* window, CHEngine::IRenderFactory* /*renderFactory*/)
{
    return CreateImpl<ImGuiLayerMTL>(window);
}

void CHModules::ImGuiFactoryMTL::Delete(CHEngine::IImGuiLayer* ptr)
{
    if (!ptr) return;
    DestroyImpl(static_cast<ImGuiLayerMTL*>(ptr));
}

CHEngine::ModuleType CHModules::ImGuiFactoryMTL::GetType() const
{
    return CHEngine::ModuleType::ImGui;
}

IMPLEMENT_MODULE_FACTORY(CHModules::ImGuiFactoryMTL)
