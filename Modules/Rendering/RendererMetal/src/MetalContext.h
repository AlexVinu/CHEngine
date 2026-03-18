#pragma once

#include <cstdint>

namespace CHModules {

    class MetalContext
    {
    public:
        bool Init(void* nsWindow, uint32_t width, uint32_t height);
        void Shutdown();

        bool BeginFrame();
        void EndFrame();

        void SetViewport(uint32_t width, uint32_t height);
        void SetClearColor(float r, float g, float b, float a);

        void* GetDevice()               const { return m_Device; }
        void* GetCommandQueue()         const { return m_CommandQueue; }
        void* GetRenderPassDescriptor() const { return m_RenderPassDescriptor; }
        void* GetCommandBuffer()        const { return m_CommandBuffer; }
        void* GetRenderEncoder()        const { return m_RenderEncoder; }

    private:
        void CreateDepthTexture();

        void* m_Device               = nullptr;
        void* m_CommandQueue         = nullptr;
        void* m_MetalLayer           = nullptr;
        void* m_CurrentDrawable      = nullptr;
        void* m_CommandBuffer        = nullptr;
        void* m_RenderEncoder        = nullptr;
        void* m_RenderPassDescriptor = nullptr;
        void* m_DepthTexture         = nullptr;
        void* m_NSWindow             = nullptr;  // NSWindow* — for backingScaleFactor

        uint32_t m_Width  = 0;
        uint32_t m_Height = 0;
        float m_ClearR = 0.18f, m_ClearG = 0.18f, m_ClearB = 0.20f, m_ClearA = 1.0f;
    };

}
