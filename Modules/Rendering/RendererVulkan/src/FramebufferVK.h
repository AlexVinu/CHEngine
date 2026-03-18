#pragma once

#include "Render/IFramebuffer.h"

namespace CHModules {

    class FramebufferVK : public CHEngine::IFramebuffer
    {
    public:
        FramebufferVK(uint32_t width, uint32_t height);
        ~FramebufferVK() override;

        void Bind()   const override;
        void Unbind() const override;
        void Resize(uint32_t width, uint32_t height) override;

        uint32_t GetColorAttachmentID() const override { return 0; }
        uint32_t GetWidth()  const override { return m_Width; }
        uint32_t GetHeight() const override { return m_Height; }

    private:
        uint32_t m_Width  = 0;
        uint32_t m_Height = 0;
        // TODO: VkFramebuffer + attachments
    };

}
