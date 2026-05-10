#pragma once

namespace CHModules {

    class FramebufferVK
    {
    public:
        FramebufferVK(uint32_t width, uint32_t height);
        ~FramebufferVK();

        void Bind()   const;
        void Unbind() const;
        void Resize(uint32_t width, uint32_t height);

        uint32_t GetColorAttachmentID() const { return 0; }
        uint32_t GetWidth()  const { return m_Width; }
        uint32_t GetHeight() const { return m_Height; }

    private:
        uint32_t m_Width  = 0;
        uint32_t m_Height = 0;
        // TODO: VkFramebuffer + attachments
    };

}
