#pragma once

#include "Render/IFramebuffer.h"

namespace CHModules {
    class FramebufferMTL : public CHEngine::IFramebuffer
    {
    public:
        FramebufferMTL(uint32_t width, uint32_t height);
        ~FramebufferMTL() override;

        void Bind()   const override;
        void Unbind() const override;
        void Resize(uint32_t width, uint32_t height) override;

        uint32_t GetColorAttachmentID() const override;
        void* GetNativeColorAttachment() const override { return m_ColorTexture; }
        uint32_t GetWidth()  const override { return m_Width; }
        uint32_t GetHeight() const override { return m_Height; }

    private:
        void CreateAttachments();
        void DestroyAttachments();

        void* m_ColorTexture = nullptr; // id<MTLTexture>
        void* m_DepthTexture = nullptr; // id<MTLTexture>
        void* m_RPDesc       = nullptr; // MTLRenderPassDescriptor*
        uint32_t m_Width = 0, m_Height = 0;
    };
}
