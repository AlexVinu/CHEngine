#include "WindowFactoryGLFW.h"

CHEngine::IWindow* CHModules::WindowFactoryGLFW::CreateIWindow(uint32_t width, uint32_t height, const char* title, CHEngine::ErrorCallbackFn errorCallbackFn, CHEngine::ERenderAPI renderApi)
{
    return CreateImpl<WindowGLFW>(width, height, title, errorCallbackFn, renderApi);
}

void CHModules::WindowFactoryGLFW::Delete(CHEngine::IWindow* ptr)
{
    DestroyImpl(static_cast<WindowGLFW*>(ptr));
}

CHEngine::ModuleType CHModules::WindowFactoryGLFW::GetType() const
{
    return CHEngine::ModuleType::Window;
}

IMPLEMENT_MODULE_FACTORY(CHModules::WindowFactoryGLFW)
