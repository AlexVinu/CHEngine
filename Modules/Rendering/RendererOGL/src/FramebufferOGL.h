#pragma once

#include "Render/IFramebuffer.h"
#include <glad/glad.h>
#include <algorithm>

namespace CHModules {

class FramebufferOGL : public CHEngine::IFramebuffer {
public:
    FramebufferOGL(uint32_t width, uint32_t height);
    ~FramebufferOGL();

    void Bind()   const override;
    void Unbind() const override;
    void Resize(uint32_t width, uint32_t height) override;

    uint32_t GetColorAttachmentID() const override { return m_ColorAttachment; }
    uint32_t GetWidth()  const override { return m_Width; }
    uint32_t GetHeight() const override { return m_Height; }

private:
    void CreateAttachments();
    void DeleteAttachments();

    GLuint   m_RendererID      = 0;
    GLuint   m_ColorAttachment = 0;
    GLuint   m_DepthAttachment = 0;
    uint32_t m_Width  = 0;
    uint32_t m_Height = 0;
};

} // namespace CHModules
