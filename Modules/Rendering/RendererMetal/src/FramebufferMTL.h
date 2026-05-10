#pragma once

namespace CHModules {
    class FramebufferMTL
    {
    public:
        FramebufferMTL(uint32_t width, uint32_t height);
        ~FramebufferMTL();

        void Bind()   const;
        void Unbind() const;
        void Resize(uint32_t width, uint32_t height);

        uint32_t GetColorAttachmentID() const;
        void* GetNativeColorAttachment() const { return m_ColorTexture; }
        uint32_t GetWidth()  const { return m_Width; }
        uint32_t GetHeight() const { return m_Height; }

    private:
        void CreateAttachments();
        void DestroyAttachments();

        void* m_ColorTexture = nullptr; // id<MTLTexture>
        void* m_DepthTexture = nullptr; // id<MTLTexture>
        void* m_RPDesc       = nullptr; // MTLRenderPassDescriptor*
        uint32_t m_Width = 0, m_Height = 0;
    };
}
