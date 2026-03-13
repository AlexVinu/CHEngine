#include "WindowHandlerFactoryGLFW.h"

inline CHEngine::IWindowHandler* CHModules::WindowHandlerFactoryGLFW::Create(const CHEngine::WindowProps& props, CHEngine::ErrorCallbackFn errorCallbackFn)
{
    return CreateImpl<WindowHandlerGLFW>(props.Width, props.Height, props.Title.c_str(), errorCallbackFn);
}

inline void CHModules::WindowHandlerFactoryGLFW::Delete(CHEngine::IWindowHandler* handler)
{
    DestroyImpl(static_cast<WindowHandlerGLFW*>(handler));
}

inline CHEngine::ModuleType CHModules::WindowHandlerFactoryGLFW::GetType() const
{
    return CHEngine::ModuleType::WindowHandler;
}

IMPLEMENT_MODULE_FACTORY(CHModules::WindowHandlerFactoryGLFW);