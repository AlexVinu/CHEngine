#pragma once

#include <cstdint>

// ─── Глобальное состояние Metal-рендерера ─────────────────────────────────
// Используется всеми классами в модуле RendererMTL для доступа к текущему
// контексту рендеринга (device, encoder, command buffer и т.д.).
// Устанавливается в MetalContext::Init / BeginFrame / EndFrame.

namespace CHModules {
namespace MTLGlobals {

    // Устройство (id<MTLDevice>) — создаётся один раз в MetalContext::Init
    inline void* g_Device          = nullptr;

    // Текущий command buffer (id<MTLCommandBuffer>) — per-frame
    inline void* g_CommandBuffer   = nullptr;

    // Текущий render command encoder (id<MTLRenderCommandEncoder>) — per-frame
    inline void* g_Encoder         = nullptr;

    // Screen drawable (id<CAMetalDrawable>) — per-frame
    inline void* g_ScreenDrawable  = nullptr;

    // Screen render pass descriptor (MTLRenderPassDescriptor*) — retained
    inline void* g_ScreenRPDesc    = nullptr;

    // Screen depth texture (id<MTLTexture>) — recreated on resize
    inline void* g_DepthTexture    = nullptr;

    // Текущий размер viewport
    inline uint32_t g_Width  = 0;
    inline uint32_t g_Height = 0;

    // Render state
    inline bool g_BlendEnabled     = false;
    inline bool g_DepthWriteEnabled = true;

    // Currently bound shader (ShaderMTL*)
    inline void* g_BoundShader     = nullptr;

    // Pixel format текущего color attachment (MTLPixelFormat as uint)
    inline uint32_t g_ColorPixelFormat = 0;
    inline uint32_t g_DepthPixelFormat = 0;

    // Pointer to MetalContext (for resize from RenderApiMTL::SetViewport)
    inline void* g_ContextPtr = nullptr;

    // Depth stencil cache lives on RendererMTL; SetDepthWrite on API sets this flag.
    inline bool g_DepthStencilStateDirty = true;

} // namespace MTLGlobals
} // namespace CHModules
