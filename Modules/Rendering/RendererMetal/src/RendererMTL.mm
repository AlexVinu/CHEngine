#include <cstdint>
#include "RendererMTL.h"
#include "MetalGlobals.h"

#include <Log/Log.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>

#import <Cocoa/Cocoa.h>

namespace CHModules
{
    void RendererMTL::Init(const CHEngine::RendererInitInfo& init_info)
    {
        // Metal.NSView содержит NSWindow* (от glfwGetCocoaWindow)
        void* nsWindow = init_info.Metal.NSView;
        if (!nsWindow) {
            CHE_CORE_CRITICAL("RendererMTL::Init — NSWindow is NULL!");
            return;
        }

        NSWindow* window = (NSWindow*)nsWindow;
        NSRect frame = [[window contentView] frame];
        uint32_t w = static_cast<uint32_t>(frame.size.width);
        uint32_t h = static_cast<uint32_t>(frame.size.height);

        if (!m_Context.Init(nsWindow, w, h)) {
            CHE_CORE_CRITICAL("RendererMTL::Init — MetalContext init failed!");
        }
    }

    void RendererMTL::Shutdown()
    {
        m_Context.Shutdown();
    }

    void RendererMTL::SetViewport(uint32_t width, uint32_t height)
    {
        m_Context.SetViewport(width, height);
    }

    bool RendererMTL::BeginFrame()
    {
        return m_Context.BeginFrame();
    }

    void RendererMTL::EndFrame()
    {
        m_Context.EndFrame();
    }

    CHEngine::RenderContextInfo RendererMTL::GetRenderContext() const
    {
        CHEngine::RenderContextInfo info;
        info.Device               = m_Context.GetDevice();
        info.CommandBuffer        = MTLGlobals::g_CommandBuffer;  // current (may change during frame)
        info.RenderEncoder        = MTLGlobals::g_Encoder;        // current encoder (updated by FBO bind/unbind)
        info.RenderPassDescriptor = m_Context.GetRenderPassDescriptor();
        return info;
    }
}
