#pragma once

#include "Render/Core/RenderAPI.h"

namespace CHModules {
    class RendererApiOGL
    {
    public:
        RendererApiOGL();
        ~RendererApiOGL() = default;

        void Init(const CHEngine::RendererInitInfo& init_info);
        void Shutdown();

        void SetClearColor(float r, float g, float b, float a);
        void Clear();

        void SetViewport(uint32_t width, uint32_t height);
        void SetBlend(bool enable);
        void SetDepthWrite(bool enable);

        void DrawFullscreenTriangle();
        void BindTextureSlot(uint32_t texID, uint32_t slot);

        // Cached GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT. Filled by Init().
        uint32_t GetUniformBufferOffsetAlignment() const { return m_UBOAlignment; }

    private:
        float  m_ClearR = 0.1f, m_ClearG = 0.1f, m_ClearB = 0.1f, m_ClearA = 1.0f;
        uint32_t m_FullscreenVAO = 0;
        uint32_t m_FullscreenVBO = 0;
        uint32_t m_UBOAlignment = 256; // safe default; replaced by Init()
    };
}
